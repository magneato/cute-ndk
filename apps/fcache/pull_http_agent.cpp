#include "errno.h"
#include "http_parser.h"
#include "pipe_buffer.h"
#include "file_manager.h"
#include "pull_http_agent.h"
#include "push_sessionmgr.h"
#include "handle_pull_file.h"
#include "pull_file_transfer.h"
#include "handle_start_transfer_data.h"

#include <ndk/logger.h>

static ndk::logger* pull_log = ndk::log_manager::instance()->get_logger("root.pull");

pull_http_agent::pull_http_agent(const pull_session_ptr &pull_ss,
                                 int64_t begin_pos,
                                 int64_t end_pos)
: timer_id_(-1),
  file_handle_(NDK_INVALID_HANDLE),
  suspend_(0),
  chunk_id_(-1),
  error_code_(0),
  connect_ok_(0),
  pull_info_ok_(0),
  pull_content_ok_(0),
  to_recv_header_(1),
  recved_first_package_(0),
  begin_pos_(begin_pos),
  end_pos_(end_pos),
  content_length_(0),
  remaind_length_(0),
  recv_bytes_(0),
  recv_buff_(0),
  pipe_buffer_(0),
  transfer_agent_(0)
{
  pull_session_ = pull_ss;
  error_desc_ = "Connect Host Failed!";
}
pull_http_agent::~pull_http_agent()
{
  if (this->file_handle_ != NDK_INVALID_HANDLE && this->file_info_)
  {
    this->file_info_->decr_fd_ref();
    this->file_handle_ = NDK_INVALID_HANDLE;
  }
  //pull_log->trace("release pull session %d", this->pull_session_->session_id());
  this->pull_session_->release();
}
int pull_http_agent::open(void *)
{
  this->recv_buff_ = new ndk::message_block(1024*32);

  if (this->get_reactor()->register_handler(this->peer().get_handle(),
                                            this, 
                                            event_handler::read_mask) == -1)
  {
    pull_log->error("[%d] register readable mask to reactor failed! [%s]", 
                    this->get_handle(),
                    strerror(errno));
    return -1;
  }

  ndk::time_value tv(3, 0);
  this->timer_id_ = this->get_reactor()->schedule_timer(this,
                                                        0,
                                                        tv);
  std::string host;
  http_parser::get_host_in_url(this->pull_session_->url_for_pulling().c_str(), 
                               host);

  std::string uri;
  http_parser::get_uri_in_url(this->pull_session_->url_for_pulling().c_str(), 
                              uri);

  this->chunk_id_ = this->begin_pos_ / PIPE_CHUNK_SIZE;
  if (this->send_reqest(uri, 
                        host, 
                        this->begin_pos_, 
                        this->end_pos_) != 0)
  {
    this->error_desc_ = "Send Request Failed!";
    return -1;
  }
  this->connect_ok_ = 1;
  return 0;
}
int pull_http_agent::send_reqest(const std::string &uri,
                                 const std::string &host,
                                 int64_t &begin_pos,
                                 int64_t &end_pos)
{
  std::ostringstream os;
  os << "GET " << uri << " HTTP/1.1\r\n"
    << "Host: " << host << "\r\n"
    << "User-Agent: Fcached/1.0\r\n"
    << "Connection: close\r\n"
    << "Accept: */*\r\n";
  if (begin_pos > 0)
  {
    os << "Range: bytes=" << begin_pos << "-";
    if (end_pos > 0) os << end_pos;
    os << "\r\n";
  }else if (end_pos > 0)
  {
    os << "Range: bytes=" << "-" << end_pos << "\r\n";
  }
  os << "\r\n";
  return this->peer().send(os.str().c_str(), os.str().length()) > 0 ? 0 : -1; 
}
int pull_http_agent::handle_input(ndk::ndk_handle )
{
  if (this->recv_buff_->space() == 0 
      && this->recv_buff_->length() > 0)
    return this->handle_content();

  int result = this->peer().recv(this->recv_buff_->wr_ptr(),
                                 this->recv_buff_->space());
  if (result < 0)
  {
    if (errno == EWOULDBLOCK)
      return 0;
    this->error_desc_ = strerror(errno);
    return -1;
  }else if (result == 0)
  {
    this->error_desc_ = "Closed By Peer";
    return -1;
  }
  //
  this->recv_buff_->wr_ptr(result);

  //
  if (this->to_recv_header_)
  {
    result = this->handle_response();
  }else
  {
    result = this->handle_content();
  }
  return result;
}
int pull_http_agent::handle_response(void)
{
  this->recv_buff_->set_eof();
  char *ptr = this->recv_buff_->data();
  if (!(ptr[0] == 'H' && ptr[1] == 'T' && ptr[2] == 'T'
      && ptr[3] == 'P' && ptr[4] == '/'))
  {
    pull_log->error("recv an invalid response [%s]", ptr);
    this->error_desc_ = "Response Is Invalid";
    return -1;
  }
  char *http_sepa = ::strstr(ptr, "\r\n\r\n");
  if (http_sepa == 0)
  {
    if (this->recv_buff_->space() == 0)
    {
      this->error_desc_ = "Response Is Too Long!";
      return -1;
    }
    pull_log->rinfo("recv an uncompletd response [%s]",
                    ptr);
    return 0;
  }

  ptr += sizeof("HTTP/x.x");

  int result = 0;
  char *status_code = ptr + ::strspn(ptr, " \t");
  int resp_code = ::atoi(status_code);
  if (status_code[0] == '2' && status_code[1] == '0')
  {
    int64_t entity_length = 0;

    // parse content length.
    char *content_length_p = http_parser::get_value(ptr,
                                                    "Content-Length",
                                                    sizeof("Content-Length") - 1,
                                                    0);
    if (content_length_p == 0)
    {
      pull_log->error("[%s] not return 'content-length'",
                      this->pull_session_->url_for_pulling().c_str());
      this->error_desc_ = "Response Is Invalid";
      return -1;
    }
    this->content_length_ = ::strtoll(content_length_p, 0, 10);
    //
    if (status_code[2] == '0') // 200 ok
    {
      if (this->begin_pos_ != 0 || this->end_pos_ != -1)
      {
        pull_log->rinfo("source service [%s] dont support partial download",
                        this->pull_session_->url_for_pulling().c_str());
        this->error_desc_ = "Original Server Dont Support Partial Download!";
        return -1;
      }
      entity_length = this->content_length_;
    }else if (status_code[2] == '6') // 206 partial
    {
      int64_t begin_pos = 0;
      int64_t end_pos = 0;
      int ret = http_parser::get_content_range(ptr,
                                               begin_pos,
                                               end_pos,
                                               entity_length);
      if (ret == -1)
      {
        pull_log->error("recv an invalid response [%s] "
                        "[Content-Range is invalid]",
                        this->recv_buff_->data());
        this->error_desc_ = "Content-range Is Invalid!";
        return -1;
      }
    }else
    {
      pull_log->error("not support response code [%c%c%c]",
                      status_code[0],
                      status_code[1],
                      status_code[2]);
      char desc[64] = {0};
      ::snprintf(desc, 
                 sizeof(desc),
                 "Original Server Return %d!",
                 resp_code);
      this->error_desc_ = desc;
      return -1;
    }
    this->to_recv_header_ = 0;
    this->pull_info_ok_ = 1;

    /**
     * We still download this chunk even if client closed, so don't check 
     * result.
     */
    handle_pull_file_ok(this->pull_session_.get(),
                        this->pull_session_->request_url(),
                        entity_length);

    if (this->content_length_ == 0)
    {
      this->pull_content_ok_ = 1;
      handle_pull_file_over(this->file_info_.get(),
                            this->pull_session_.get());
      return -1;
    }

    //
    this->remaind_length_ = this->content_length_;

    this->file_info_ = 
      file_manager::instance()->find(this->pull_session_->request_url());

    if (!this->file_info_)
    {
      pull_log->error("find file [%s] failed!",
                      this->pull_session_->request_url().c_str());
      this->error_desc_ = "Find File Info Failed When Pull File!";
      return -1;
    }

    this->file_handle_ = this->file_info_->file_handle();
    if (this->file_handle_ == NDK_INVALID_HANDLE)
    {
      this->file_handle_ = ::open(this->file_info_->filename().c_str(), 
                             O_CREAT | O_RDWR | O_TRUNC, 
                             0644);
      if (this->file_handle_ == NDK_INVALID_HANDLE)
      {
        pull_log->error("open file [%s] failed! [%s]",
                        this->file_info_->filename().c_str(),
                        strerror(errno));
        if (errno == ENOENT)
          file_manager::instance()->remove(this->file_info_->url());
        return -1;
      }
      this->file_info_->file_handle(this->file_handle_);
    }
    this->file_info_->incr_fd_ref();

    //
    this->pipe_buffer_ = this->create_pipe_buffer(this->remaind_length_,
                                                  this->chunk_id_++);
    //this->remaind_length_ -= this->pipe_buffer_->size();
    pull_log->trace("create [%s] pipe buffer %d",
                    this->pull_session_->request_url().c_str(),
                    this->pipe_buffer_->chunk_id());
    this->pipe_buffer_->open();
    this->file_info_->insert_pipe_buffer(this->pipe_buffer_);

    // Check remaind data.
    int http_header_len = http_sepa + 4 - this->recv_buff_->data();
    int attach_content_len = this->recv_buff_->length() - http_header_len;
    if (attach_content_len > 0)
    {
      this->recv_buff_->rd_ptr(http_header_len);

      result = this->handle_content();
    }else  // end of `if (attach_content_len > 0)'
    {
      this->recv_buff_->reset();
      result = 0;
    }
  }else // end of `if (status_code[0] == '2')'
  {
    // 
    int resp_code = ::atoi(status_code);
    char desc[64] = {0};
    ::snprintf(desc, 
               sizeof(desc),
               "Original Server Return %d!",
               resp_code);
    this->error_desc_ = desc;
    result = -1;
  }
  return result;
}
int pull_http_agent::handle_content()
{
  if (this->pipe_buffer_ == 0)
  {
    this->pipe_buffer_ = this->file_info_->find_pipe_buffer(this->chunk_id_);
    if (this->pipe_buffer_)
    {
      pull_log->trace("find a pipe buffer %d", this->chunk_id_);
      this->pipe_buffer_->open();
      ++this->chunk_id_;
    }else
    {
      pull_log->error("find pipe buffer %d failed!", this->chunk_id_);
      return -1;
    }
  }
  size_t bytes_to_write = this->recv_buff_->length();
  assert(bytes_to_write > 0);
  size_t copy_bytes = this->pipe_buffer_->write(this->recv_buff_->rd_ptr(),
                                                bytes_to_write);
  if (copy_bytes > 0)
  {
    this->recv_buff_->rd_ptr(copy_bytes);   // !!

    if (this->recv_buff_->length() == 0)  // !!
      this->recv_buff_->reset();

    this->recv_bytes_ += copy_bytes;
  }
  if (copy_bytes != bytes_to_write)
  {
    this->pipe_buffer_->close(0);
    this->pipe_buffer_ = 0;

    //
    int push_sid = this->pull_session_->push_session_id();
    push_session_ptr push_ss = push_sessionmgr::instance()->find(push_sid);
    if (!push_ss)
    {
      pull_log->trace("push session [%d] had released, then pull session"
                      " [%d] release",
                      push_sid,
                      this->pull_session_->session_id());
      return -1;
    }
    
    pull_log->trace("recv %lld bytes , suspend %d",
                    this->recv_bytes_,
                    this->get_handle());
    // copy failed, pipe buffer is full, then suspend this http session.
    int result = this->get_reactor()->suspend_handler(this->get_handle());
    if (result != 0)
    {
      pull_log->error("suspend handle %d failed!", 
                      this->get_handle());
      return -1;
    }
    this->suspend_ = 1;
  }
  if (!this->recved_first_package_)
  {
    this->recved_first_package_ = 1;
    push_session_ptr ps = 
      push_sessionmgr::instance()->find(this->pull_session_->push_session_id());
    if (ps)
    {
      int64_t end_pos = ps->range_end_pos();
      if (end_pos == -1)
        end_pos = this->file_info_->length() - 1;
      this->transfer_agent_ = 
        new pull_file_transfer(this->get_handle(),
                               ps->range_begin_pos(), 
                               end_pos - ps->range_begin_pos() + 1,
                               this->remaind_length_);
      if (this->transfer_agent_->open(this->file_info_) != 0)
      {
        delete this->transfer_agent_;
        this->transfer_agent_ = 0;
        return -1;
      }
      handle_start_transfer_data(this->transfer_agent_,
                                 ps->session_id(),
                                 ps->output_bdwidth_limit(),
                                 ps->range_begin_pos(),
                                 ps->range_end_pos(),
                                 ps->content_length(),
                                 ps->client());
    }else
    {
      return -1;
    }
  }else if (this->suspend_)
  {
    this->transfer_agent_->start_transfer();
    this->suspend_ = 0;
  }
  if (this->recv_bytes_ == this->content_length_)
  {
    pull_log->trace("pull file %s completed %lld bytes",
                    this->pull_session_->request_url().c_str(),
                    this->recv_bytes_);
    this->pipe_buffer_->close(this->pull_session_->session_id());
    this->pipe_buffer_ = 0;

    this->transfer_agent_->start_transfer();

    // over !!
    handle_pull_file_over(this->file_info_.get(),
                          this->pull_session_.get());
    return -1;
  }
  return 0;
}
int pull_http_agent::handle_timeout(const void *, const ndk::time_value &)
{
  return 0;
}
int pull_http_agent::handle_close(ndk::ndk_handle h, ndk::reactor_mask mask)
{
  if (!this->pull_info_ok_)
  {
    pull_log->error("pull file [%s] failed!", 
                    this->pull_session_->request_url().c_str());
    this->error_desc_ = std::string("Pull File Failed! ") 
      + "[" 
      + this->error_desc_
      + "]";
    handle_pull_file_failed(this->error_desc_.c_str(),
                            this->pull_session_.get(),
                            0);
  }
  super::handle_close (h, mask);
  return 0;
}
pipe_buffer *pull_http_agent::create_pipe_buffer(int64_t entity_length,
                                                 int chunk_id)
{
  /**
   * Create a pipe buffer.
   * Suspend this socket until pipe buffer is full.
   *
   */
  size_t pbuff_size = PIPE_CHUNK_SIZE;
  if (pbuff_size > entity_length)
    pbuff_size = entity_length;
  pipe_buffer *pbuffer = new pipe_buffer(chunk_id,
                                         pbuff_size, 
                                         this->file_info_);
  return pbuffer;
}
