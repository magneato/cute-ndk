#include "serial_file_transfer.h"
#include "buffer_manager.h"

#include <ndk/logger.h>

static ndk::logger *file_io_log = ndk::log_manager::instance()->get_logger("root.file_io");

extern ndk::asynch_file_io **g_aio_task;
extern buffer_manager *file_io_cache_mgr;

#define ONCE_TRANSFER_PACKET_SIZE   4096

static int new_count = 0;
serial_file_transfer::serial_file_transfer(uint64_t start_pos,
                                           uint64_t content_length)
  : handle_(NDK_INVALID_HANDLE),
  aio_inprogress_(0),
  eof_(0),
  file_io_error_(0),
  cancel_pendding_(0),
  send_bytes_(0),
  buffer_size_(0),
  reserve_buffer_size_(0),
  offset_(start_pos),
  content_length_(content_length),
  start_pos_(start_pos),
  transfer_bytes_(0),
  buffer_(0),
  reserve_buffer_(0)
{
  ++new_count;
}

serial_file_transfer::~serial_file_transfer()
{
  --new_count;
  printf("%p %p c = %d\n", this->buffer_, this->reserve_buffer_, new_count);
  if (this->buffer_)
  {
    file_io_cache_mgr->free(this->buffer_);
    this->buffer_ = 0;
  }
  if (this->reserve_buffer_)
  {
    file_io_cache_mgr->free(this->reserve_buffer_);
    this->reserve_buffer_ = 0;
  }
  if (this->handle_ != NDK_INVALID_HANDLE)
  {
    if (this->fileinfo_)
      this->fileinfo_->decr_fd_ref();
    this->handle_ = NDK_INVALID_HANDLE;
  }
}
int serial_file_transfer::open(const fileinfo_ptr &fileinfo)
{
  this->transfer_bytes_ = 0;
  this->send_bytes_ = 0;
  this->fileinfo_ = fileinfo;
  this->handle_ = fileinfo->file_handle();
  if (this->handle_ == NDK_INVALID_HANDLE)
  {
    this->handle_ = ::open(fileinfo->filename().c_str(), O_RDONLY);
    if (this->handle_ == NDK_INVALID_HANDLE)
    {
      file_io_log->error("open file [%s] failed! [%s]",
                         fileinfo->filename().c_str(),
                         strerror(errno));
      return -1;
    }
    this->fileinfo_->file_handle(this->handle_);
  }
  this->fileinfo_->incr_fd_ref();

  this->start_read();

  return 0;
}
int serial_file_transfer::start_read()
{
  if (this->aio_inprogress_ || this->reserve_buffer_ != 0)
    return -1;
  file_io_log->debug("start read offset = %lld",
                     this->offset_);

  this->reserve_buffer_ = (char *)file_io_cache_mgr->malloc();
  assert(this->reserve_buffer_ != 0);
  if (g_aio_task[0]->start_aio(this->handle_,
                               file_io_cache_mgr->block_size(),
                               this->offset_,
                               this->reserve_buffer_,
                               this,
                               ndk::AIO_READ,
                               0) != 0)
  {
    file_io_log->warning("start aio failed! [h = %d][file: %s]",
                         this->handle_,
                         this->fileinfo_->filename().c_str());
    file_io_cache_mgr->free(this->reserve_buffer_);
    this->reserve_buffer_ = 0;
    this->reserve_buffer_size_ = 0;
    return -1;
  }
  ++this->aio_inprogress_;
  return 0;
}
int serial_file_transfer::transfer_data(ndk::ndk_handle handle,
                                        int max_size, 
                                        int &transfer_bytes)
{
  //
  if (this->buffer_ == 0) 
  {
    ndk::guard<ndk::thread_mutex> g(this->buffer_mtx_);
    if (this->aio_inprogress_) return 0;
    // the first calling.
    if (this->reserve_buffer_ == 0)
    {
      this->start_read();
      return 0; //
    }else
    {
      this->buffer_                 = this->reserve_buffer_;
      this->buffer_size_            = this->reserve_buffer_size_;
      this->send_bytes_             = 0;

      // clear !!
      this->reserve_buffer_         = 0;
      this->reserve_buffer_size_    = 0;
    }
  }
  
  int result = 0;
  int bytes_to_send = 0;
  while (transfer_bytes < max_size 
         && this->send_bytes_ < this->buffer_size_
         && this->transfer_bytes_ < this->content_length_)
  {
    bytes_to_send = this->buffer_size_ - this->send_bytes_;
    result = ndk::send(handle,
                       this->buffer_ + this->send_bytes_, 
                       bytes_to_send > ONCE_TRANSFER_PACKET_SIZE ? 
                       ONCE_TRANSFER_PACKET_SIZE : bytes_to_send,
                       0);
    if (result >= 0)
    {
      this->transfer_bytes_ += result;  // statistic total flux.
      this->send_bytes_     += result;  // 
      transfer_bytes        += result;  // statistic flux in once calling.
    }else if (errno != EWOULDBLOCK)
      return -1;
    else 
      return 0;
  }
  if (this->transfer_bytes_ == this->content_length_)
    return transfer_bytes;

  // prefetch the next cache block.
  if (this->send_bytes_ >= this->buffer_size_/2)
  {
    if (this->send_bytes_ == this->buffer_size_)
    {
      file_io_cache_mgr->free(this->buffer_);
      this->buffer_       = 0;
      this->buffer_size_  = 0;
      this->send_bytes_   = 0;
    }
    ndk::guard<ndk::thread_mutex> g(this->buffer_mtx_);
    if (this->eof_ || this->file_io_error_)
    {
      if (this->send_bytes_ == this->buffer_size_)
        return -1;
    }else if (this->aio_inprogress_ == 0 || this->reserve_buffer_ == 0)
      this->start_read();
  }
  return transfer_bytes;
}
void serial_file_transfer::handle_aio_read(const ndk::aio_opt_t *aio_result)
{
  ndk::guard<ndk::thread_mutex> g(this->buffer_mtx_);
  --this->aio_inprogress_;
  if (this->cancel_pendding_ == 1)
  {
    this->close_i();
    return ;
  }
  if (aio_result->errcode() == 0)
  {
    if (aio_result->bytes_completed() == 0) // EOF
    {
      file_io_log->debug("read end of file [%s]", this->fileinfo_->filename().c_str());
      this->eof_ = 1;
      return ;
    }
    this->offset_     += aio_result->bytes_completed();
    // must lock.
    assert(this->reserve_buffer_ == aio_result->buffer());
    this->reserve_buffer_ = const_cast<char *>(aio_result->buffer());
    this->reserve_buffer_size_ = aio_result->bytes_completed();
    return ;
  }else // end of `if (aio_result->errcode() == 0)'
  {
    file_io_log->error("read file [%s] offset = %lld errcode = %d",
                    this->fileinfo_->filename().c_str(),
                    this->offset_,
                    aio_result->errcode());
    this->file_io_error_ = 1;
  }
  return ;
}
int serial_file_transfer::close()
{
  ndk::guard<ndk::thread_mutex> g(this->buffer_mtx_);
  return this->close_i();
}
int serial_file_transfer::close_i()
{
  int result = ndk::AIO_CANCELED;
  if (this->aio_inprogress_ != 0)
    g_aio_task[0]->cancel_aio(this->handle_);
  if (result == ndk::AIO_CANCELED)
  {
    this->cancel_pendding_ = 0;
    delete this;
  }else if (result == ndk::AIO_NOTCANCELED)
  {
    this->cancel_pendding_ = 1;
  }else
    assert(0);
  return 0;
}
