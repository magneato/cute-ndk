#include "pipe_buffer.h"
#include "file_manager.h"

#include <ndk/guard.h>
#include <ndk/logger.h>

static ndk::logger *file_io_log = ndk::log_manager::instance()->get_logger("root.file_io");

extern ndk::asynch_file_io **g_aio_task;

pipe_buffer::pipe_buffer(int chunk_id,
                         size_t content_length, 
                         const file_info_ptr &fileinfo)
  : handle_(NDK_INVALID_HANDLE),
  chunk_id_(chunk_id),
  status_(ST_SUSPEND),
  ref_count_(0),
  wr_ptr_(0),
  buffer_size_(content_length),
  buffer_(0),
  file_info_(fileinfo)
{
  buffer_ = new char[content_length];
  this->handle_ = this->file_info_->file_handle();
  assert(this->handle_ != NDK_INVALID_HANDLE);
  this->file_info_->incr_fd_ref();
}
pipe_buffer::~pipe_buffer()
{
  if (this->buffer_)
  {
    delete []this->buffer_;
    this->buffer_ = 0;
  }

  if (this->handle_ != NDK_INVALID_HANDLE && this->file_info_)
  {
    this->file_info_->decr_fd_ref();
    this->handle_ = NDK_INVALID_HANDLE;
  }
}
int pipe_buffer::open()
{
  ndk::guard<ndk::thread_mutex> g(this->buffer_mtx_);
  ++this->ref_count_;
  return 0;
}
size_t pipe_buffer::write(const char *bf, size_t bf_len)
{
  ndk::guard<ndk::thread_mutex> g(this->buffer_mtx_);
  if (this->wr_ptr_ >= this->buffer_size_)
    return 0;

  size_t len = bf_len > (this->buffer_size_ - this->wr_ptr_) 
    ? this->buffer_size_ - this->wr_ptr_ 
    : bf_len;

  std::memcpy(this->buffer_ + this->wr_ptr_, 
              bf,
              len);

  this->wr_ptr_ += len;  // 

  return len;
}
size_t pipe_buffer::read(char *bf, size_t r_offset, size_t bytes_to_read)
{
  ndk::guard<ndk::thread_mutex> g(this->buffer_mtx_);
  if (bf == 0 || bytes_to_read <= 0 || this->wr_ptr_ <= r_offset)
    return 0;

  size_t len = bytes_to_read > (this->wr_ptr_ - r_offset) ?
    this->wr_ptr_ - r_offset : bytes_to_read;

  std::memcpy(bf, this->buffer_ + r_offset, len);

  return len;
}
void pipe_buffer::handle_aio_write(const ndk::aio_opt_t *aio_result)
{
  if (aio_result->errcode() == 0)
  {
    assert(aio_result->bytes_completed() == this->buffer_size_);
    this->file_info_->set_bitmap(this->chunk_id_);
    this->file_info_->mtime(::time(0));
    if (this->file_info_->check_file_completed() == true)
      this->file_info_->status(file_info::ST_COMPLETED);
  }else
  {
    file_io_log->error("write pipe buffer failed [%s]",
                       strerror(aio_result->errcode()));
  }

  ndk::guard<ndk::thread_mutex> g(this->buffer_mtx_);
  if (this->ref_count_ == 0)
  {
    this->file_info_->release_pipe_buffer(this->chunk_id_);
    delete this;
  }
}
int pipe_buffer::close(const int )
{
  ndk::guard<ndk::thread_mutex> g(this->buffer_mtx_);

  --this->ref_count_;

  if (this->ref_count_ == 0)
  {
    if (this->wr_ptr_ == 0)
    {
      this->file_info_->release_pipe_buffer(this->chunk_id_);
      delete this;
      return 0;
    }

    file_io_log->trace("write [%s] pipe buffer [%d] to disk! [size = %d]",
                       this->file_info_->url().c_str(),
                       this->chunk_id_,
                       this->buffer_size_);
    int aio_id = -1;
    int result = g_aio_task[0]->start_aio(this->handle_,
                                          &aio_id,
                                          this->buffer_size_,
                                          this->chunk_id_ * PIPE_CHUNK_SIZE,
                                          this->buffer_,
                                          this,
                                          ndk::AIO_WRITE,
                                          0,
                                          5);
    if (result == -1)
    {
      file_io_log->error("aio failed!");
      //insert to pipe buffer pending queue.
        ++this->ref_count_;
      return -1;
    }
  }
  return 0;
}
