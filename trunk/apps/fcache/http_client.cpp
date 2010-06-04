#include "http_client.h"
#include "http_session.h"
#include "http_sessionmgr.h"
#include "file_manager.h"
#include "dispatch_data_task.h"
#include "mem_cache_transfer.h"
#include "serial_file_transfer.h"

#include <ndk/logger.h>
#include <ndk/cache_manager.h>

#include <fstream>
#include <iomanip>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>

extern std::string g_doc_root;
extern dispatch_data_task *g_dispatch_data_task;
extern ndk::cache_manager<std::string, ndk::thread_mutex> *g_cache_manager;

static ndk::logger*net_log = ndk::log_manager::instance()->get_logger("root.net");

http_client::~http_client()
{
  if (this->session_id_ != -1)
  {
    g_dispatch_data_task[this->dispatch_data_task_idx_].delete_client(this->session_id_);
    http_session *hs = 
      http_sessionmgr::instance()->remove(this->session_id_);
    if (hs) delete hs;
    net_log->debug("remove sessionid %d", this->session_id_);
    this->session_id_ = -1;
  }
  if (this->recv_buff_)
  {
    this->recv_buff_->release();
    this->recv_buff_ = 0;
  }

  if (this->file_handle_ != NDK_INVALID_HANDLE)
  {
    ::close(this->file_handle_);
    this->file_handle_ = NDK_INVALID_HANDLE;
  }
  if (this->cache_obj_)
  {
    g_cache_manager->release(this->cache_obj_);
    net_log->rinfo("%p refer count = %d", 
                   this->cache_obj_,
                   this->cache_obj_->refcount());
  }
  if (this->content_length_ > 0 && this->send_bytes_ == this->content_length_)
  {
    ndk::time_value diff = ndk::time_value::gettimeofday() - this->begin_time_;
    net_log->rinfo("send data completely! [%d bytes used %lu msecs]", 
                   this->content_length_,
                   diff.msec());
  }
  this->cache_obj_ = 0;
}
int http_client::open(void *)
{
  char addr[32] = {0};  // 
  this->remote_addr_.addr_to_string(addr, sizeof(addr));
  net_log->debug("new connection [%s][%d]", addr, this->peer().get_handle());
  this->recv_buff_ = new ndk::message_block(1024*1024);
  if (this->get_reactor() 
      && this->get_reactor()->register_handler(this->peer().get_handle(),
                                               this, 
                                               event_handler::read_mask) == -1)
  {
    net_log->error("[%d] register readable mask to reactor failed! [%s]", 
                   this->get_handle(),
                   strerror(errno));
    return -1;
  }
  this->begin_time_ = ndk::time_value::gettimeofday();
  ndk::time_value tv(3, 0);
  this->timer_id_ = this->get_reactor()->schedule_timer(this,
                                                        0,
                                                        tv);
  ndk::socket::set_sndbuf(this->peer().get_handle(),
                          32*1024);
  assert(this->timer_id_ != -1);
  return this->timer_id_ == -1 ? -1 : 0;
}
int http_client::handle_input(ndk::ndk_handle )
{
  int result = this->peer().recv(this->recv_buff_->wr_ptr(),
                                 this->recv_buff_->size() -
                                 this->recv_buff_->length());
  if (result < 0)
  {
    net_log->error("socket %d error [%s]!",
                   this->peer().get_handle(),
                   strerror(errno));
    if (errno == EWOULDBLOCK)
      return 0;
    return -1;
  }else if (result == 0)
  {
    net_log->debug("socket %d closed by peer!", 
                   this->peer().get_handle());
    return -1;
  }
  if (this->recv_msg_ok_)
  {
    net_log->error("handle %d had recv a msg!",
                   this->peer().get_handle());
    return -1;
  }

  this->recv_buff_->wr_ptr(result);

  return this->handle_data();
}
int http_client::handle_timeout(const void *, const ndk::time_value &now)
{
  if (this->recv_msg_ok_)
    return 0;
  ndk::time_value tv = now - this->begin_time_;
  net_log->error("[%d] recv request timeout [escape %d.%d]",
                 this->peer().get_handle(),
                 tv.sec(),
                 (int)tv.msec());
  return -1;
}
int http_client::handle_close(ndk::ndk_handle , ndk::reactor_mask m)
{
  assert(m == ndk::event_handler::read_mask 
         || m == ndk::event_handler::timer_mask
         || m == ndk::event_handler::write_mask
         || m == ndk::event_handler::all_events_mask);
  this->destroy();
  return 0;
}
int http_client::handle_data()
{
  this->recv_buff_->set_eof();
  char *ptr = this->recv_buff_->data();
  char *p = ::strstr(ptr, "\r\n\r\n");
  if (p == 0)
  {
    net_log->rinfo("recv an uncompletd request");
    return 0;
  }
  char *uri_begin_p = ptr + 4;
  char *uri_end_p = ::strstr(uri_begin_p, " HTTP/1.");
  if (uri_end_p == 0)
  {
    this->response_client(400, 0, 0, "Not found 'HTTP/1.x'");
    return -1;
  }
  char *host_p = ::strstr(uri_end_p, "Host: ");
  if (host_p == 0)
  {
    this->response_client(400, 0, 0, "Not found 'Host' header");
    return -1;
  }
  char *host_p_end = ::strstr(host_p + sizeof("Host:"), "\r\n");
  if (host_p_end == 0)
  {
    this->response_client(400, 0, 0, "Not found '\\r\\n' after 'Host'");
    return -1;
  }
  std::string host(host_p + sizeof("Host:"),
                   host_p_end - (host_p + sizeof("Host:")));

  char *range_p = ::strstr(uri_end_p, "Range: bytes=");
  int64_t start_pos = 0;
  int64_t end_pos = -1;
  int partial = 0;
  if (range_p != 0)
  {
    partial = 1;
    char *range_p_end = ::strstr(range_p + sizeof("Range: bytes=") - 1, "\r\n");
    if (range_p_end == 0)
    {
      this->response_client(400, 0, 0, "Not found '\\r\\n' after 'Range'");
      return -1;
    }
    char *endptr = 0;
    if (*(range_p + sizeof("Range: bytes=") - 1) != '-')
    {
      start_pos = ::strtoll(range_p + sizeof("Range: bytes=") - 1, &endptr, 10);
      char *sep = ::strchr(range_p + sizeof("Range: bytes=") - 1 + 1, '-');
      if (sep != 0 && isdigit(*(sep + 1)))  // 123456-123
      {
        end_pos = ::strtoll(sep + 1, &endptr, 10);
        if (end_pos == 0) end_pos = -1;
      }
    }
  }

  std::string uri(uri_begin_p, uri_end_p - uri_begin_p);
  std::string url = "http://" + host + uri;

  std::string filename = g_doc_root + uri;

  fileinfo_ptr fileinfo = file_manager::instance()->find(url);
  if (!fileinfo)  
  {
    ndk::ndk_handle fd = ::open(filename.c_str(), O_RDONLY);
    struct stat st;
    if (fd != NDK_INVALID_HANDLE 
        && ::fstat(fd, &st) != -1 
        && ((st.st_mode & S_IFMT) == S_IFREG))
    {
      fileinfo = fileinfo_ptr(new file_info());
      fileinfo->length(st.st_size);
      fileinfo->url(url);
      fileinfo->filename(filename);
      fileinfo->mtime(st.st_mtime);
      file_manager::instance()->insert(fileinfo);
      if (fd != NDK_INVALID_HANDLE) ::close(fd);
    }else
    {
      if (fd != NDK_INVALID_HANDLE) ::close(fd);
      if (errno == ENOENT)
        this->response_client(404, 0, 0, strerror(errno));
      else
        this->response_client(503, 0, 0, "-");
      if (fd != NDK_INVALID_HANDLE) ::close(fd);
      return -1;
    }
  }

  this->recv_msg_ok_ = 1;

  if (end_pos == -1)
    end_pos = fileinfo->length() - 1;

  int64_t content_length = end_pos - start_pos + 1;

  int result = 0;
  transfer_agent *trans_agent = 0;
  if (fileinfo->length() < 1024*1024*16)
  {
    trans_agent = new mem_cache_transfer(start_pos, content_length);
    result = trans_agent->open(fileinfo);
    if (result != 0)
    {
      // cache manager is full.
      delete trans_agent;
      trans_agent = 0;
    }
  }

  // direct read file from disk.
  if (trans_agent == 0)
  {
    trans_agent = new serial_file_transfer(start_pos, content_length);
    if (trans_agent->open(fileinfo) != 0)
    {
      delete trans_agent;
      this->response_client(503, 0, 0, "-");
      return -1;
    }
  }
  if (partial == 1)
    result = this->response_client(206, start_pos, end_pos, "HIT");
  else
    result = this->response_client(200, start_pos, end_pos, "HIT");
  if (result != -1)
  {
    this->session_id_ = http_sessionmgr::instance()->alloc_sessionid();
    http_session *hs = new http_session(this->session_id_);
    hs->client(this);
    http_sessionmgr::instance()->insert(hs);
    dispatch_data_task::dispatch_job *job = 
      new dispatch_data_task::dispatch_job;
    job->session_id = this->session_id_;
    job->transfer_agent_ = trans_agent;
    job->client = this;
    job->content_length_ = content_length;
    g_dispatch_data_task[0].push_job(job);
  }
  return result;
}
int http_client::response_client(int status,
                                 int64_t start_pos,
                                 int64_t end_pos,
                                 const std::string &desc)
{
  std::ostringstream os;
  // header
  switch(status)
  {
  case 200:
    os << "HTTP/1.0 200 OK\r\n";
    break;
  case 206:
    os << "HTTP/1.0 206 Partial Content\r\n";
    break;
  case 404:
    os << "HTTP/1.0 404 Not Found\r\n";
    break;
  case 400:
    os << "HTTP/1.0 400 Bad Request\r\n";
    break;
  case 503:
    os << "HTTP/1.0 503 Service Unavailable\r\n";
    break;
  default:
    assert(0);
    break;
  }

  os << "Server: fcache\r\n";

  // 
  if (status == 200 || status == 206)
  {
    os << "Accept-Ranges: bytes\r\n"
      << "Connection: close\r\n"
      << "Content-Length: " << end_pos - start_pos + 1 << "\r\n";
    if (status == 206)
    {
      os << "Content-Range: bytes " 
        << start_pos 
        << "-" 
        << end_pos 
        << "/" 
        << end_pos - start_pos + 1 
        << "\r\n";
    }
  }else
  {
    os << "Connection: close\r\n";
  }
  os << "\r\n";
  int result = this->peer().send(os.str().c_str(), os.str().length());
  return result < 0 ? -1 : 0;
}
int http_client::show_status()
{
#if 0
  //g_cache_manager->check();  // for debug
  STRACE("");
  static int v1 = 10, v2 = 10, v3 = 10, v4 = 10; 
  static int v5 = 10, v6 = 10, v7 = 10, v8 = 10, v9 = 10;
  static char requests[]     = "requests";
  static int  requests_w     = v1;
  static char finished[]     = "finished";
  static int  finished_w     = v2;
  static char hit[]          = "hit";
  static int  hit_w          = v3;
  static char miss[]         = "miss";
  static int  miss_w         = v4;
  static char hit_rate[]     = "hit_rate";
  static int  hit_rate_w     = v5;
  static char miss_rate[]    = "miss_rate";
  static int  miss_rate_w    = v6;
  static char payload[]      = "payload";
  static int  payload_w      = v7;
  static char put_obj[]      = "put_obj";
  static int  put_obj_w      = v8;
  static char drop_obj[]     = "drop_obj";
  static int  drop_obj_w     = v9;

  std::ostringstream  ostr;
  ostr << std::setfill(' ')
    << std::setiosflags(std::ios::left);
  ostr << std::setw(requests_w) << requests
    << std::setw(finished_w) << finished
    << std::setw(hit_w) << hit
    << std::setw(miss_w) << miss
    << std::setw(hit_rate_w) << hit_rate
    << std::setw(miss_rate_w) << miss_rate
    << std::setw(payload_w) << payload
    << std::setw(put_obj_w) << put_obj
    << std::setw(drop_obj_w) << drop_obj
    << std::endl;
  int hrate  = 0;
  if (g_finished > 0)
    hrate = int(int64_t(g_hit * 100) / int64_t(g_request_ok));
  int mrate = 0;
  if (g_finished > 0)
    mrate = int(int64_t(g_miss * 100) / int64_t(g_request_ok));
  ostr << std::setw(requests_w) << g_requests
    << std::setw(finished_w) << g_finished
    << std::setw(hit_w) << g_hit
    << std::setw(miss_w) << g_miss
    << std::setw(hit_rate_w) << hrate
    << std::setw(miss_rate_w) << mrate
    << std::setw(payload_w) << g_payload
    << std::setw(put_obj_w) << g_put_obj
    << std::setw(drop_obj_w) << g_drop_obj
    << std::endl;
  std::ostringstream os;
  os << "HTTP/1.0 200 OK\r\n"
    << "Server: fcache\r\n"
    << "Accept-Ranges: bytes\r\n"
    << "Connection: close\r\n"
    << "Content-Length: " << ostr.str().length() << "\r\n"
    << "\r\n"
    << ostr.str().c_str();
  this->peer().send(os.str().c_str(), os.str().length());
#endif
  return -1;
}
