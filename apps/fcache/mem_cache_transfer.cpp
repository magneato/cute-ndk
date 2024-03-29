#include "mem_cache_transfer.h"
#include "errno.h"

#include <sys/mman.h>
#include <ndk/logger.h>
#include <ndk/cache_manager.h>
#include <ndk/asynch_file_io.h>

#define ONCE_TRANSFER_PACKET_SIZE   4096

static ndk::logger *memcache_log = ndk::log_manager::instance()->get_logger("root.memcache");

extern int g_cache_mem_used;
extern int64_t g_hit_cache;
extern ndk::asynch_file_io **g_aio_task;
extern ndk::cache_manager<std::string, ndk::thread_mutex> *g_cache_manager;

class observer : public ndk::cache_object_observer
{
public:
  virtual int drop(ndk::cache_object *cobj)
  {
    g_cache_mem_used -= cobj->size();
    ::munmap(cobj->data(), cobj->size());
    return 0;
  }
};
observer *g_cache_object_observer = new observer();

mem_cache_transfer::mem_cache_transfer(int64_t begin_pos,
                                       int64_t content_length)
: offset_(begin_pos),
  content_length_(content_length),
  begin_pos_(begin_pos),
  transfer_bytes_(0),
  cache_obj_(0)
{
}
mem_cache_transfer::~mem_cache_transfer()
{
  if (this->cache_obj_)
  {
    g_cache_manager->release(this->cache_obj_);
    this->cache_obj_ = 0;
  }
}
int mem_cache_transfer::open(const file_info_ptr &finfo)
{
  this->cache_obj_ = g_cache_manager->get(finfo->url());
  if (this->cache_obj_ == 0)
  {
    ndk::ndk_handle fd = ::open(finfo->filename().c_str(), O_RDONLY);
    if (fd != NDK_INVALID_HANDLE)
    {
      void *data = ::mmap(0, 
                          finfo->length(), 
                          PROT_READ, 
                          MAP_SHARED, 
                          fd, 
                          0);
      ::close(fd);
      if (data == 0 || g_cache_manager->put(finfo->url(), 
                                            data, 
                                            finfo->length(), 
                                            g_cache_object_observer,
                                            this->cache_obj_) != 0)
      {
        if (data)
        {
          memcache_log->rinfo("put cache to manager failed!");
          ::munmap(data, finfo->length());
        }else
          memcache_log->error("mmap failed![%s]", strerror(errno));
        return -1;
      }else
      {
        memcache_log->rinfo("hit [%s] failed! [refcount = %d][cobj = %p]", 
                            finfo->url().c_str(),
                            this->cache_obj_->refcount(),
                            this->cache_obj_);
        assert(this->cache_obj_ != 0);  // for debug
        g_cache_mem_used += finfo->length();
        // cache new file ok.
      }
    }else 
    {
      memcache_log->error("open failed! [%s]", strerror(errno));
      return -1;
    }
  }else // end of `if (this->cache_obj_ == 0)'
  {
    ++g_hit_cache;
  }
  return 0;
}
int mem_cache_transfer::transfer_data(ndk::ndk_handle handle,
                                      int max_size, 
                                      int &transfer_bytes)
{
  int result = 0;
  int64_t bytes_to_send = 0;
  while (transfer_bytes < max_size 
         && this->transfer_bytes_ < this->content_length_)
  {
    bytes_to_send = this->content_length_ - this->transfer_bytes_;
    result = ndk::send(handle,
                       (char *)this->cache_obj_->data() 
                       + int(this->begin_pos_)
                       + int(this->transfer_bytes_), 
                       bytes_to_send > int64_t(ONCE_TRANSFER_PACKET_SIZE) ? 
                       int64_t(ONCE_TRANSFER_PACKET_SIZE) : bytes_to_send,
                       0);
    if (result >= 0)
    {
      this->transfer_bytes_ += result;  // statistic total flux.
      transfer_bytes        += result;  // statistic flux in once calling.
    }else if (errno != EWOULDBLOCK)
    {
      memcache_log->error("trnasfer data failed! [h = %d][%s]",
                          handle,
                          strerror(errno));
      return -CLT_ERR_SEND_DATA_FAILED;
    }else 
      return 0;
  }
  return transfer_bytes;
}
int mem_cache_transfer::close()
{
  return 0;
}
