// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-05-14 14:57
 * Brief    : For testing multi thread file I/O.
 */
//========================================================================

#include <deque>
#include <string>
#include <fstream>
#include <cassert>
#include <iomanip>
#include <signal.h>
#include <dirent.h>
#include <iostream>
#include <algorithm>
#include <ndk/task.h>
#include <ndk/guard.h>
#include <ndk/types.h>
#include <ndk/logger.h>
#include <ndk/condition.h>

static ndk::logger *main_log = ndk::log_manager::instance()->get_logger("root.aio_thread");

std::deque<std::string> file_list;
ndk::thread_mutex file_list_mtx;

int concurrent_aio_requests_keeping = 64;
int aio_threads  = 3;

std::string file_path;
int block_size = 1024;

class asynch_file_io;
asynch_file_io *aio_task = 0;

uint64_t g_total_bytes = 0; 
ndk::thread_mutex g_total_bytes_mtx;

ndk::time_value begin_time;
ndk::time_value end_time;

enum
{
  AIO_READ,

  AIO_WRITE,

  AIO_CANCEL,

  // error code
  AIO_WOULDBLOCK,

  AIO_CANCELED,

  AIO_NOTCANCELED
};

#define MAX_AIO_QUEUE_LIST_SIZE  4096

class aio_opt_t;
class asynch_handler
{
public:
  virtual void handle_aio_read(const aio_opt_t *) { }

  virtual void handle_aio_write(const aio_opt_t *) { }
};
class aio_opt_t
{
  friend class asynch_file_io;
public:
  aio_opt_t()
    : handle_(NDK_INVALID_HANDLE),
    opcode_(-1),
    errcode_(0),
    priority_(0),
    i_nbytes_(0),
    o_nbytes_(0),
    offset_(0),
    buffer_(0),
    aio_handler_(0),
    next_(0)
  { }

  inline ndk::ndk_handle handle() const
  { return this->handle_; }

  inline int priority() const
  { return this->priority_; }

  inline size_t bytes_completed() const
  { return this->o_nbytes_; }

  inline uint64_t offset() const
  { return this->offset_; }

  inline int errcode() const
  { return this->errcode_; }

  inline char *buffer() const
  { return this->buffer_; }
protected:
  void reset()
  {
    this->handle_   = NDK_INVALID_HANDLE;
    this->opcode_   = -1;
    this->errcode_  =  0;
    this->priority_ = 0;
    this->i_nbytes_ = 0;
    this->o_nbytes_ = 0;
    this->offset_   = 0;
    this->buffer_   = 0;
    this->aio_handler_ = 0;
    this->next_ = 0;
  }
protected:
  ndk::ndk_handle handle_;
  int opcode_;
  int errcode_;
  int priority_;
  size_t i_nbytes_;
  size_t o_nbytes_;
  uint64_t offset_;
  char *buffer_;
  asynch_handler *aio_handler_;

  aio_opt_t *next_;
};
class asynch_file_io: public ndk::task_base
{
public:
  asynch_file_io()
    : queue_list_size_(0),
    queue_list_(0),
    queue_list_mtx_(),
    not_empty_cond_(queue_list_mtx_),
    running_list_(0),
    free_list_(0)
  { }

  int open(int thr_num = 2)
  {
    return this->activate(ndk::thread::thr_join, thr_num);
  }
  int start_aio(ndk::ndk_handle handle,
                size_t nbytes,
                uint64_t offset,
                char *buff,
                asynch_handler *handler,
                int optcode,
                int priority)
  {
    assert(nbytes > 0);
    assert(handle != NDK_INVALID_HANDLE);
    assert(handler != 0);
    assert(optcode == AIO_READ || optcode == AIO_WRITE);
    
    aio_opt_t *aioopt = this->alloc_aio_opt();
    if (aioopt == 0) return AIO_WOULDBLOCK;
    
    aioopt->aio_handler_ = handler;
    aioopt->handle_   = handle;
    aioopt->i_nbytes_ = nbytes;
    aioopt->buffer_   = buff;
    aioopt->offset_   = offset;
    aioopt->opcode_   = optcode;
    aioopt->priority_ = priority;

    return this->enqueue_aio_request(aioopt);
  }
  /**
   * cancel an asynchronous I/O request
   * Return AIO_CANCELED the requested operation(s) were canceled.
   *        AIO_NOTCANCELED at least one of the requested operation(s) 
   *        cannot be canceled because it is in progress.
   */
  int cancel_aio(ndk::ndk_handle handle)
  {
    assert(handle != NDK_INVALID_HANDLE);
    int result = AIO_CANCELED;
    {
      ndk::guard<ndk::thread_mutex> g(this->queue_list_mtx_);
      if (this->queue_list_ != 0) 
      {
        aio_opt_t *itor = this->queue_list_;
        aio_opt_t *prev = itor;
        while (itor != 0)
        {
          if (itor->handle_ == handle)
          {
            aio_opt_t *p = itor;
            if (itor == prev)
            {
              itor = itor->next_;
              this->queue_list_ = itor;
              prev = itor;
            }else
            {
              prev->next_ = itor->next_; 
              itor = prev->next_;
            }
            this->free_aio_opt(p);
          }else
          {
            prev = itor;
            itor = itor->next_;
          }
        }
      }
    }

    // 
    ndk::guard<ndk::thread_mutex> g(this->running_list_mtx_);
    if (this->running_list_ == 0) return result;

    aio_opt_t *itor = this->running_list_;
    while (itor != 0)
    {
      if (itor->handle_ == handle)
      {
        result = AIO_NOTCANCELED;
        break;
      }
    }

    return result;
  }
protected:
  virtual int svc()
  {
    while (1)
    {
      aio_opt_t *running_list = 0;
      {
        ndk::guard<ndk::thread_mutex> g(this->queue_list_mtx_);
        while (this->queue_list_ == 0)
        {
          this->not_empty_cond_.wait(0);
        }
        aio_opt_t *tail = this->queue_list_->next_;
        aio_opt_t *prev = this->queue_list_;
        running_list = this->queue_list_;
        for (int i = 1; i < 5 && tail != 0; ++i)
        {
          prev = tail;
          tail = tail->next_;
        }
        prev->next_ = 0;
        this->queue_list_ = tail;
      }
      this->handle_aio_requests(running_list);
    }
    return 0;
  }
  void handle_aio_requests(aio_opt_t *running_list)
  {
    this->enqueue_to_running_list(running_list);
    for (aio_opt_t *aioopt = running_list; aioopt != 0; aioopt = aioopt->next_)
    {
      int result = 0;
      void (asynch_handler::*callback)(const aio_opt_t *) = 0;
      switch(aioopt->opcode_)
      {
      case AIO_READ:
        result = ::pread(aioopt->handle_,
                         (void*)aioopt->buffer_,
                         aioopt->i_nbytes_,
                         aioopt->offset_);
        callback = &asynch_handler::handle_aio_read;
        break;
      case AIO_WRITE:
        result = ::pwrite(aioopt->handle_,
                          (const void*)aioopt->buffer_,
                          aioopt->i_nbytes_,
                          aioopt->offset_);
        callback = &asynch_handler::handle_aio_write;
        break;
      default:
        assert(0);
      }
      if (result >= 0)
      {
        aioopt->errcode_  = 0;
        aioopt->o_nbytes_ = result;
      }else
      {
        aioopt->errcode_  = errno;
      }
      this->dequeue_from_running_list(aioopt, callback);
    }
    this->free_aio_opt(running_list);
  }
  int enqueue_aio_request(aio_opt_t *aioopt)
  {
    ndk::guard<ndk::thread_mutex> g(this->queue_list_mtx_);
    if (this->queue_list_ == 0)
    {
      this->queue_list_ = aioopt;
    }else
    {
      // Simply enqueue it after the running one according to the priority.
      aio_opt_t *itor = this->queue_list_;
      while (itor->next_ && aioopt->priority_ <= itor->next_->priority_)
      {
        itor = itor->next_;
      }

      aioopt->next_ = itor->next_;
      itor->next_ = aioopt;
    }

    this->not_empty_cond_.signal();

    return 0;
  }
  void enqueue_to_running_list(aio_opt_t *running_list)
  {
    ndk::guard<ndk::thread_mutex> g(this->running_list_mtx_);
    aio_opt_t *itor = running_list;
    if (this->running_list_ == 0)
      this->running_list_ = running_list;
    else
    {
      this->running_list_->next_;
      aio_opt_t *tail = this->running_list_;
      while (tail->next_ != 0) tail = tail->next_;
      tail->next_ = running_list; 
    }
  }
  void dequeue_from_running_list(aio_opt_t *aioopt, 
                                 void (asynch_handler::*callback)(const aio_opt_t *))
  {
    ndk::guard<ndk::thread_mutex> g(this->running_list_mtx_);
    (aioopt->aio_handler_->*callback)(aioopt);
    assert(this->running_list_ != 0);

    aio_opt_t *itor = this->running_list_;
    aio_opt_t *prev = itor;
    while (itor != 0)
    {
      if (itor == aioopt)
      {
        if (itor == prev)
        {
          itor = itor->next_;
          this->running_list_ = itor;
          prev = itor;
        }else
        {
          prev->next_ = itor->next_; 
          itor = prev->next_;
        }
      }else
      {
        prev = itor;
        itor = itor->next_;
      }
    }
  }
  aio_opt_t *alloc_aio_opt()
  {
    ndk::guard<ndk::thread_mutex> g(this->free_list_mtx_);
    if (this->queue_list_size_ > MAX_AIO_QUEUE_LIST_SIZE)
      return 0;
    if (this->free_list_ == 0)
      this->free_list_ = new aio_opt_t();

    aio_opt_t *aioopt = this->free_list_;
    this->free_list_ = this->free_list_->next_;
    aioopt->reset();
    ++this->queue_list_size_;
    return aioopt;
  }
  void free_aio_opt(aio_opt_t *p)
  {
    ndk::guard<ndk::thread_mutex> g(this->free_list_mtx_);
    aio_opt_t *last = p;
    int c = 0;
    for (; last->next_ != 0; last = last->next_) ++c;
    last->next_ = this->free_list_;
    this->free_list_ = p;
    this->queue_list_size_ -= c;
  }
protected:
  size_t queue_list_size_;

  aio_opt_t *queue_list_;  // single link
  ndk::thread_mutex queue_list_mtx_;
  ndk::condition<ndk::thread_mutex> not_empty_cond_;

  aio_opt_t *running_list_;
  ndk::thread_mutex running_list_mtx_;

  aio_opt_t *free_list_;
  ndk::thread_mutex free_list_mtx_;
};


class aio_test : public asynch_handler
{
public:
  aio_test()
    : bytes_to_read_(0),
    open_ok_(-1),
    handle_(NDK_INVALID_HANDLE),
    read_bytes_(0),
    offset_(0),
    buffer_(0)
  { }

  ~aio_test()
  {
    if (this->handle_ != NDK_INVALID_HANDLE)
    {
      ::close(this->handle_);
      this->handle_ == NDK_INVALID_HANDLE;
    }
    if (this->buffer_ != 0)
    {
      delete [] this->buffer_;
      this->buffer_ = 0;
    }
    if (this->open_ok_  == 0)
      aio_test::open_next_file();
  }
  int open(const char *file)
  {
    this->file_name_ = file;
    this->bytes_to_read_ = block_size*1024;
    this->read_bytes_ = 0;
    this->buffer_ = new char[this->bytes_to_read_];
    this->offset_ = 0;
    this->handle_ = ::open(file, O_RDONLY);
    if (this->handle_ == NDK_INVALID_HANDLE) return -1;
    int result = aio_task->start_aio(this->handle_, 
                                     this->bytes_to_read_,
                                     this->offset_,
                                     this->buffer_,
                                     this,
                                     AIO_READ,
                                     0);
    this->open_ok_ = result;
    main_log->debug("open file [%s] ok! [%d]", file, result);
    return result;
  }
  virtual void handle_aio_read(const aio_opt_t *aio_result)
  {
    if (aio_result->errcode() == 0)
    {
      if (aio_result->bytes_completed() == 0) // EOF
      {
         main_log->debug("read end of file [%s]",
                         this->file_name_.c_str());
         delete this;
         return ;
      }
      this->offset_     += aio_result->bytes_completed();
      this->read_bytes_ += aio_result->bytes_completed();
      {
        ndk::guard<ndk::thread_mutex> g(g_total_bytes_mtx);
        g_total_bytes += aio_result->bytes_completed();
      }

      int result = aio_task->start_aio(this->handle_,
                                       this->bytes_to_read_,
                                       this->offset_,
                                       this->buffer_,
                                       this,
                                       AIO_READ,
                                       0);
      if (result != 0)
      {
        main_log->rinfo("start aio %d failed!", this->handle_);
        delete this;
      }
      return ;
    }else
    {
      main_log->error("read file [%s] offset = %lld errcode = %d",
                      this->file_name_.c_str(),
                      this->offset_,
                      aio_result->errcode());
      int result = aio_task->cancel_aio(this->handle_);
      if (result == AIO_CANCELED)
      {
        delete this;
        return ;
      }else if (result == AIO_NOTCANCELED)
      {
        return ;
      }else
        assert(0);
    }
  }
  static void open_next_file()
  {
    std::string file_name;
    {
      ndk::guard<ndk::thread_mutex> g(file_list_mtx);
      if (!file_list.empty())
      {
        end_time.update();
        ndk::time_value diff = end_time - begin_time;
        ndk::guard<ndk::thread_mutex> g(g_total_bytes_mtx);
        main_log->rinfo("read %lld bytes used %d.%d sec",
                        g_total_bytes,
                        (int)diff.sec(),
                        (int)(diff.usec()/1000));
      }else
      {
        main_log->rinfo("no files!");
        return ;
      }
      file_name = file_list.front();
      file_list.pop_front();
    }
    aio_test *at = new aio_test();
    if (at->open(file_name.c_str()) != 0)
    {
      main_log->rinfo("open failed !");
      delete at;
    }
  }
protected:
  int bytes_to_read_;
  int open_ok_;
  ndk::ndk_handle handle_;
  int read_bytes_;
  uint64_t offset_;
  char *buffer_;
  std::string file_name_;
};
void load_dir(const char *dir, std::deque<std::string> &file_list)
{
  DIR *dirp;
  struct dirent *dp;

  if ((dirp = opendir(dir)) == NULL) 
  {
    fprintf(stderr, "open dir %s failed!\n", dir);
    return;
  }
  while ((dp = readdir(dirp)) != NULL) 
  {
    if (dp->d_type != DT_REG && dp->d_type != DT_LNK) continue;

    if (::strcmp(dp->d_name, ".") == 0 
        || ::strcmp(dp->d_name, "..") == 0)
      continue;
    file_list.push_back(file_path + "/" + std::string(dp->d_name));
  }
  closedir(dirp);
}
void print_usage()
{
  printf("Usage: aio_thread [OPTION...]\n\n");
  printf("  -k  number            Number of requests keeping\n"); 
  printf("  -d  dir               Directry\n");
  printf("  -t  number            The number of aio threads\n");
  printf("  -n  block size(KB)    The size of block read in once\n");
  printf("\n");
}
int main (int argc, char *argv[])
{
  if (argc == 1)
  {
    print_usage();
    return 0;
  }
  int c = -1;
  const char *opt = "k:d:t:n:";
  extern int optind, optopt;
  while ((c = getopt(argc, argv, opt)) != -1)
  {
    switch(c)
    {
    case 'k':
      concurrent_aio_requests_keeping = ::atoi(optarg);
      break;
    case 'd':
      file_path = optarg;
      break;
    case 't':
      aio_threads = ::atoi(optarg);
      break;
    case 'n':
      block_size = ::atoi(optarg);
      break;
    case ':':
      fprintf(stderr, "Option -%c requires an operand\n", optopt);
      return -1;
    case '?':
    default:
      print_usage();
      return 0;
    }
  }
  if (ndk::log_manager::instance()->init("logger-cfg.ini") != 0)
  {
    fprintf(stderr, "init logger failed\n");
    return 0;
  }
  load_dir(file_path.c_str(), file_list);

  aio_task = new asynch_file_io();
  aio_task->open(aio_threads);
  ndk::sleep(ndk::time_value(1, 0));
  begin_time.update();
  for (int i = 0; i < concurrent_aio_requests_keeping; i++)
  {
    aio_test::open_next_file();
  }

  ndk::sleep(ndk::time_value(10000, 0));
  return 0;
}

