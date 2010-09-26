#include "reactor_event_handler.h"
#include "pull_file_transfer.h"
#include "buffer_manager.h"
#include "pipe_buffer.h"

#include <ndk/reactor.h>
#include <ndk/logger.h>

extern buffer_manager *file_io_cache_mgr;

static ndk::logger* pull_log = ndk::log_manager::instance()->get_logger("root.pull");

pull_file_transfer::pull_file_transfer(int pull_sid,
                                       ndk::ndk_handle pull_file_handle,
                                       int64_t begin_pos,
                                       int64_t bytes_to_transfer,
                                       int64_t remaind_length)
: pull_session_id_(pull_sid),
  chunk_id_(begin_pos/PIPE_CHUNK_SIZE),
  pull_file_handle_(pull_file_handle),
  state_(STOP_TRANSFER),
  first_chunk_(1),
  rd_offset_(0),
  send_bytes_(0),
  buffer_size_(0),
  buffer_(0),
  begin_pos_(begin_pos),
  bytes_to_transfer_(bytes_to_transfer),
  transfer_bytes_(0),
  remaind_length_(remaind_length),
  pipe_buffer_(0)
{
  this->buffer_ = (char *)file_io_cache_mgr->malloc();
}
pull_file_transfer::~pull_file_transfer()
{
  if (this->buffer_)
  {
    file_io_cache_mgr->free(this->buffer_);
    this->buffer_ = 0;
  }
  if (this->pipe_buffer_)
  {
    this->pipe_buffer_->close(0);
    this->pipe_buffer_ = 0;
  }
}
int pull_file_transfer::open(const file_info_ptr &finfo)
{
  this->file_info_      = finfo;
  pull_log->trace("open:: finding pipe buffer [%d]", this->chunk_id_);
  this->pipe_buffer_ = this->file_info_->find_pipe_buffer(this->chunk_id_);
  assert(this->pipe_buffer_ != 0);
  if (this->pipe_buffer_ == 0)
    return -1;
  this->pipe_buffer_->open();  // !!!
  pull_log->trace("find a pipe buffer [%d] and open it",
                  this->chunk_id_);
  return 0;
}
int pull_file_transfer::transfer_data(ndk::ndk_handle h,
                                      int max_size,
                                      int &transfer_bytes)
{
  {
    ndk::guard<ndk::thread_mutex> g(this->state_mtx_);
    if (this->state_ == STOP_TRANSFER)
      return 0;
  }

  assert(this->pipe_buffer_ != 0);

  int result = 0;
  if (this->buffer_size_ == 0)
  {
    this->read_pipe_buffer();
    if (result <= 0) return result;
  }

  int bytes_to_send = 0;
  while (transfer_bytes < max_size 
         && this->send_bytes_ < this->buffer_size_
         && this->transfer_bytes_ < this->bytes_to_transfer_)
  {
    bytes_to_send = this->buffer_size_ - this->send_bytes_;

    result = ndk::send(h,
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
    {
      pull_log->error("trnasfer data failed! [h = %d][%s]",
                      h,
                      strerror(errno));
      return -1;
    }else 
    {
      pull_log->debug("errno = wouldblockd");
      return 0;
    }
  }
  if (this->transfer_bytes_ == this->bytes_to_transfer_)
  {
    pull_log->trace("transfer completed! %lld",
                    this->transfer_bytes_);
    this->pipe_buffer_->close(0); // ??????????????
    this->pipe_buffer_ = 0;
    this->state_ = STOP_TRANSFER;
    return transfer_bytes;
  }

  // prefetch the next cache block.
  if (this->send_bytes_ == this->buffer_size_)
  {
    this->send_bytes_  = 0;
    this->buffer_size_ = 0;
    result = this->read_pipe_buffer();
    if (result <= 0) return result;
  }
  return transfer_bytes;
}
int pull_file_transfer::read_pipe_buffer()
{
  //
  if (this->first_chunk_)
  {
    this->rd_offset_   += this->begin_pos_ % PIPE_CHUNK_SIZE;
    this->first_chunk_  = 0;
  }
  size_t read_bytes = this->pipe_buffer_->read(this->buffer_, 
                                               this->rd_offset_, 
                                               file_io_cache_mgr->block_size());
  if (read_bytes == 0)
  {
    // ensure pipe buffer is empty.
    if (!this->pipe_buffer_->eof())
      return 0;

    pull_log->trace("reamaind length = %lld transfer bytes = %lld",
                    this->remaind_length_,
                    this->transfer_bytes_);
    this->remaind_length_ -= this->pipe_buffer_->size();
    if (this->remaind_length_ == 0)
    {
      //return 0;
    }
    this->pipe_buffer_->close(0); 
    this->pipe_buffer_ = 0;

    this->rd_offset_ = 0;

    ++this->chunk_id_;

    this->state_ = STOP_TRANSFER;

    /**
     * pull agent pull file is very fast, so we create pipe in
     * transfer that pipe buffer can't be released by pull-agent
     * before transfer use it.
     */
    this->pipe_buffer_ = this->create_pipe_buffer();
    pull_log->trace("create pipe buffer [%d]",
                    this->pipe_buffer_->chunk_id());
    this->pipe_buffer_->open();
    this->file_info_->insert_pipe_buffer(this->pipe_buffer_);

    pull_log->trace("to resume handle %d", this->pull_file_handle_);
    // pipe is empty
    int result = this->resume_handler();
    if (result != 0)
    {
      pull_log->error("resume handle %d failed!", 
                      this->pull_file_handle_);
      this->pipe_buffer_->close(0);
      this->pipe_buffer_ = 0;
    }
    return result;
  }else
  {
    this->buffer_size_  = read_bytes;
    this->rd_offset_   += read_bytes;
  }
  return read_bytes;
}
pipe_buffer *pull_file_transfer::create_pipe_buffer()
{
  size_t pbuff_size = PIPE_CHUNK_SIZE;
  if (int64_t(pbuff_size) > this->remaind_length_)
    pbuff_size = this->remaind_length_;
  pipe_buffer *pbuffer = new pipe_buffer(this->chunk_id_,
                                         pbuff_size, 
                                         this->file_info_);
  return pbuffer;
}
int pull_file_transfer::resume_handler()
{
  if (this->pull_session_id_ != -1)
  {
    return ndk::reactor::instance()->notify(reactor_event_handler::instance(),
                                            new notify_event(NOTIFY_RESUME_HANDLE, 
                                                             this->pull_session_id_));
  }
  return -1;
}
int pull_file_transfer::close()
{
  return 0;
}
