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
  ndk::inet_addr remote_addr;
  if (this->peer().get_remote_addr(remote_addr) != 0)
  {
    log->error("get [fd = %d] remote address failed!",
               this->get_handle());
    return -1;
  }
  char addr[32] = {0};
  remote_addr.get_host_addr(addr, sizeof(addr));
  log->rinfo("new connection from [%s:%d]", 
             addr, remote_addr.port_number());

  this->recv_buff_ = new ndk::message_block(4096);
  if (this->get_reactor() == 0)
    this->set_reactor(ndk::reactor::instance());
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
	char *content_len_pos = ::strstr(this->recv_buff_->rd_ptr(), "Content-Length:");
	if (content_len_pos == 0)
	{
		log->error("not found `Content-Length:' [buff = %s]", 
                        this->recv_buff_->data());
		return -1;
	}
	int content_len = 0;
	::sscanf(content_len_pos, "Content-Length:%d\r\n", &content_len);
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
  char *p = ::strchr(data + 5, "HTTP/1.");
  if (p == 0)
  {
    log->error("get an invalid request [%s]", data);
    return -1;
  }
  http_request *req = new http_request;
  req->uri.assign(data + 4, (p - 1) - (data + 4));

  // @step 2. parse modified time.
  char *modified_since_p = ::strstr(p, if_modified_since);
  char *unmodified_since_p = ::strstr(p, if_unmodified_since);
  char *unless_modified_since_p = ::strstr(p, if_unless_modified_since);
  char *mpos = 0;
  if (modified_since_p)
  {
    mpos = modified_since_p + sizeof(if_modified_since);
  }else if (unmodified_since)
  {
    mpos = unmodified_since_p + sizeof(if_unmodified_since);
  }else if (unless_modified_since_p)
  {
    mpos = unless_modified_since_p + sizeof(if_unless_modified_since);
  }
  if (mpos)
  {
    std::string in_modified_time;
    char *eof = strstr(mpos, "\r\n");
    if (eof)
      in_modified_time.assign(mpos, eof - mpos);
    else
    {
      log->error("get an invalid request [%s]", data);
      return -1;
    }
    std::string::size_type p1 = req->uri.find_last_of('?');
    std::string filename;
    if (p1 == std::string::npos)
      filename = req->uri;
    else
      filename = req->uri.substr(0, p1);
    filename = svc_config::instance()->fs_root() + filename;
    fileinfo finfo = file_manager::instance()->find_file(filename);
    if (!finfo)
    {
      ndk_stat st;
      memset((void*)&st, 0, sizeof(st));
      if (ndk_os::stat(filename.c_str(), &st) == 0)
      {
        finfo = new fileinfo(filename);
        finfo->mtime = st.st_mtime;
        finfo->modified_time = help::get_gmt(finfo->mtime);
        finfo->file_size     = st.st_size;
        file_manager::instance()->insert_file(finfo);
      }else
      {
        this->peer()->send("404");
        return -1;
      }
    } // if (!finfo)
    if (in_modified_time == finfo->modified_time)
    {
      this->peer()->send("304");
      return -1;
    }
  } // if (mpos)
  
  // @step 3. parse range.
  char *range_pos = ::strstr(p, "Range");
  if (range_pos)
  {
    ::sscanf(range_pos, "Range: bytes=%lld-%lld\r\n",
             &(req->begin_pos),
             &(req->end_pos));
  }else
  {
    range_pos = ::strstr(p, "Content-Range");
    if (range_pos)
    {
      sscanf(range_pos, "Content-Range: bytes=%lld-%lld\r\n",
             &(req->begin_pos),
             &(req->end_pos));
    }
  }

  // @step 4. insert into I/O dispatch queue.
  return 0;
}


