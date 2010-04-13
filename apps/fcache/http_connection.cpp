#include "http_connection.h"

static ndk::ndk_log *log = ndk::log_manager::instance()->get_logger("http");

http_connection::http_connection()
  : recv_buff_(0)
{
}
http_connection::~http_connection()
{
  if (this->recv_buff_)
    this->recv_buff_->release();
  this->recv_buff_ = 0;
}
int http_connection::open(void *arg)
{
  char addr[32] = {0};
  this->remote_addr_.addr_to_string(addr, sizeof(addr));
  log->rinfo("new connection from [%s", addr);

  this->recv_buff_ = new ndk::message_block(4096);
  return this->get_reactor()->register_handler(this, 
                                               ndk::event_handler::read_mask);
}
int http_connection::handle_input(ndk::ndk_handle )
{
	int result = this->peer().recv(this->recv_buff_->wr_ptr(),
                                 this->bytes_to_recv_);
	if (result <= 0)
	{
		log->debug("peer closed or error [%s]", strerror(ndk_os::last_error()));
		return -1;
	}
	this->recv_buff_->wr_ptr(result);

	char *http_sepa = ::strstr(this->recv_buff_->rd_ptr(), "\r\n\r\n");
	if (http_sepa == 0)
	{
		log->warning("not found `\\r\\n\\r\\n' [buff = %s]", 
                        this->recv_buff_->data());
		this->bytes_to_recv_ -= this->recv_buff_->length();
		return 0;
	}
#if 0
	char *content_len_pos = ::strstr(this->recv_buff_->rd_ptr(), "Content-Length:");
	if (content_len_pos == 0)
	{
		log->error("not found `Content-Length:' [buff = %s]", 
                        this->recv_buff_->data());
		return -1;
	}
	int content_len = ::atoi(content_len_pos + sizeof("Content-Length:")); 
	int http_header_len = http_sepa - this->recv_buff_->data() + 4;

	int total_len = http_header_len + content_len;
	if (total_len > this->recv_buff_->size())
	{
		log->error("message body[%s] is too long",
                       this->recv_buff_->data());
		return -1;
	}
	if (this->recv_buff_->length() < total_len)
	{
		log->debug ("message body[%s] is uncompleted header len = %d,"
                        " total len = %d, recv_bytes = %d",
				this->recv_buff_->data(), 
        http_header_len, 
        total_len, 
        this->recv_buff_->length());
		this->bytes_to_recv_ = total_len - this->recv_buff_->length();
		return 0;
	}
#endif
	this->recv_buff_->set_eof();
	return this->handle_data (this->recv_buff_);
}
static const char if_modified_since[] = "If-Modified-Since:"; 
static const char if_unmodified_since[] = "If-Unmodified-Since:";
static const char if_unless_modified_since[] = "Unless-Modified-Since:";  
int http_connection::handle_data(ndk::message_block *mb)
{
  // @step 1. parse uri
  char *data = mb->data();
  if (data[0] != 'G' || data[1] != 'E' || data[2] != 'T' 
      || data[3] != ' ' || data[4] != '/')
  {
    log->error("get an invalid request [%s]", data);
    return -1;
  }
  char *uri_begin = data + 4;
  char *p = ::strstr(uri_begin, "HTTP/1.");
  if (p == 0)
  {
    log->error("get an invalid request [%s]", data);
    return -1;
  }
  char *headers = ::strstr(p, "\r\n");
  if (header == 0)
  {
    log->error("get an invalid request [%s]", data);
    return -1;
  }
  headers += 2;

  this->http_request_ = new http_request;
  this->http_request_->uri.assign(uri_begin, (p - 1) - uri_begin);

  // @step 2. parse range.
  char *range_pos = ::strstr(headers, "Range");
  if (range_pos)
  {
    range_pos += sizeof("Range: bytes=") - 1;
  }else if ((range_pos = ::strstr(headers, "Content-Range")))
  {
    range_pos += sizeof("Content-Range: bytes=") - 1;
  }
  //
  if (range_pos)
  {
    if (*range_pos == '-')
    {
      if (*(range_pos + 1) != '\r')
        this->http_request_->end_pos = ::atoll(range_pos + 1);
    }else
    {
      if (*range_pos != '\r')
        this->http_request_->begin_pos = ::atoll(range_pos);
      char *sp = ::strchr(range_pos, '-');
      if (sp && *(sp + 1) != '\r')
          this->http_request_->end_pos = ::atoll(sp + 1);
    }
  }

  // @step 4. insert into I/O dispatch queue.
  ndk::inet_addr local_addr;
  if (this->peer().get_local_addr(local_addr) != 0)
  {
    log->error("get local addr failed!", strerror(errno));
    return -1;
  }
  return s_net_interface_scheduler->dispatch_request(local_addr, 
                                                     this->http_request_);
}
