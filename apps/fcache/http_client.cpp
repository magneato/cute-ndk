#include "errno.h"
#include "predefined.h"
#include "http_client.h"
#include "http_parser.h"
#include "push_session.h"
#include "file_manager.h"
#include "buffer_manager.h"
#include "push_sessionmgr.h"
#include "handle_pull_file.h"
#include "dispatch_data_task.h"
#include "mem_cache_transfer.h"
#include "serial_file_transfer.h"
#include "handle_start_transfer_data.h"

#include <ndk/logger.h>
#include <ndk/cache_manager.h>

#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>

//
int64_t   g_requests        = 0;
int64_t   g_cache_file_requests = 0;
int64_t   g_disk_file_reqeusts  = 0;
int64_t   g_finished        = 0;
int64_t   g_hit_cache       = 0;
int64_t   g_hit_disk        = 0;
int       g_payload         = 0;
int       g_cache_mem_used  = 0;
//

extern buffer_manager *file_io_cache_mgr;
extern size_t g_min_mem_cache_size;
extern size_t g_max_mem_cache_size;
extern int g_concurrency_num;
extern int g_listen_port;
extern std::string g_doc_root;
extern dispatch_data_task *g_dispatch_data_task;
extern ndk::cache_manager<std::string, ndk::thread_mutex> *g_cache_manager;

static ndk::logger* net_log = ndk::log_manager::instance()->get_logger("root.net");
static ndk::logger* access_log = ndk::log_manager::instance()->get_logger("root.access");

const char *HTTP_METHOD[] = {
  "UNKNOW",
  "GET",
  "HEAD"
};

http_client::~http_client()
{
  if (this->http_method_ == HTTP_HEAD
      || this->http_method_ == HTTP_GET)
    ++g_finished;

  if (this->session_id_ != -1)
  {
    --g_payload;
    g_dispatch_data_task[this->dispatch_data_task_idx_].delete_client(this->session_id_);
    push_sessionmgr::instance()->release(this->session_id_);
    net_log->trace("remove sessionid %d", this->session_id_);
    this->session_id_ = -1;
  }
  if (this->recv_buff_)
  {
    this->recv_buff_->release();
    this->recv_buff_ = 0;
  }

  char st[32] = {0};
  char et[32] = {0};
  ndk::time_value tv_s = this->begin_time_;
  tv_s.sec(0);
  ndk::date_time(this->begin_time_.sec()).to_str(st, sizeof(st));

  ndk::time_value now = ndk::time_value::gettimeofday();
  ndk::time_value tv_e = now;
  tv_e.sec(0);
  ndk::date_time(now.sec()).to_str(et, sizeof(et));

  char addr[32] = {0};  // 
  this->remote_addr_.get_host_addr(addr, sizeof(addr));
  char log[1024] = {0};
#if __WORDSIZE == 64
# define  INT64_P_FMT "%ld"
#else
# define  INT64_P_FMT "%lld"
#endif
  int len = ::snprintf(log, sizeof(log),
                       "\"%s.%03lu\" \"%s.%03lu\" %s %s %s %d "
                       INT64_P_FMT " " INT64_P_FMT
                       " \"%s\"\n",
                       st,
                       tv_s.msec(),
                       et,
                       tv_e.msec(),
                       addr,
                       HTTP_METHOD[this->http_method_],
                       this->uri_.c_str(),
                       this->response_code_,
                       this->content_length_,
                       this->transfer_bytes_,
                       this->session_desc_.c_str());

  access_log->puts(log, len);
}
int http_client::open(void *)
{
  char addr[32] = {0};  // 
  this->remote_addr_.addr_to_string(addr, sizeof(addr));
  net_log->trace("new connection [%s][%d]", addr, this->peer().get_handle());
  this->recv_buff_ = new ndk::message_block(1024*4);
  if (this->get_reactor() 
      && this->get_reactor()->register_handler(this->peer().get_handle(),
                                               this, 
                                               event_handler::read_mask) == -1)
  {
    net_log->error("[%d] register readable mask to reactor failed! [%s]", 
                   this->peer().get_handle(),
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
int http_client::handle_input(ndk::ndk_handle h)
{
  int result = this->peer().recv(this->recv_buff_->wr_ptr(),
                                 this->recv_buff_->space());
  if (result < 0)
  {
    if (errno == EWOULDBLOCK)
      return 0;
    if (this->recv_msg_ok_ == 0)
      this->session_desc_ = strerror(errno);
    net_log->error("socket %d error [%s]!", h, strerror(errno));
    return -1;
  }else if (result == 0)
  {
    if (this->recv_msg_ok_ == 0)
      this->session_desc_ = strerror(errno);
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

  if (this->recv_buff_->space() == 0)
  {
    this->response_to_client(413);
    this->session_desc_ = "Request entity too large!";
    net_log->error("Request entity too large!");
    return -1;
  }

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
  this->response_to_client(408);
  this->session_desc_ = "Request Timeout";
  return -1;
}
int http_client::handle_close(ndk::ndk_handle h, ndk::reactor_mask m)
{
  assert(m == ndk::event_handler::read_mask 
         || m == ndk::event_handler::timer_mask
         || m == ndk::event_handler::write_mask
         || m == ndk::event_handler::connect_mask
         || m == 0
         || m == ndk::event_handler::all_events_mask);
  if (m == ndk::event_handler::all_events_mask)
  {
    net_log->error("connection [%d] reset!", h);
    this->session_desc_ = "Connection Reset";
  }
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
    net_log->warning("recv an uncompletd request");
    return 0;
  }
  // check method
  if (ptr[0] == 'G' && ptr[1] == 'E' && ptr[2] == 'T')
    this->http_method_ = HTTP_GET;
  else if (ptr[0] == 'H' && ptr[1] == 'E' && ptr[2] == 'A' && ptr[3] == 'D')
    this->http_method_ = HTTP_HEAD;
  else
  {
    this->session_desc_ = "Unsupported HTTP Method!";
    net_log->error("unsupport http method [%c%c%c%c]", 
                   ptr[0],
                   ptr[1],
                   ptr[2],
                   ptr[3]
                   );
    this->response_to_client(501);
    return -1;
  }

  // 
  char *uri_begin_p = ptr;
  if (this->http_method_ == HTTP_GET)
    uri_begin_p += sizeof("GET");
  else if (this->http_method_ == HTTP_HEAD)
    uri_begin_p += sizeof("HEAD");

  // get uri
  char *uri_end_p = 0;
  uri_begin_p = http_parser::get_uri(uri_begin_p, &uri_end_p);
  if (uri_begin_p == 0)
  {
    this->session_desc_ = "Parse Uri Failed";
    this->response_to_client(400);
    return -1;
  }
  this->uri_.assign(uri_begin_p, uri_end_p);

  // interface.
  if (this->uri_.length() == 1)
  {
    this->show_status();
    return -1;
  }
  return this->handle_request(uri_end_p);
}
int http_client::handle_request(const char *http_header)
{
  int result = 0;

  // parse params
  char *url_p = ::strchr(this->uri_.c_str(), '?');
  if (url_p == 0 || *(url_p + 1) == 0)
  {
    this->session_desc_ = "Not Found URL Params In URI";
    this->response_to_client(400);
    return -1;
  }

  std::string url;
  int bandwidth = 0;
  result = this->parse_params(url_p + 1, bandwidth, url);
  if (result != 0)
  {
    this->session_desc_ = "Invalid Params";
    this->response_to_client(400);
    return -1;
  }

  // check payload.
  if (g_payload > g_concurrency_num)
  {
    this->session_desc_ = "Out Of Payload";
    this->response_to_client(503);
    return -1;
  }

  net_log->trace("request file [%s]", url.c_str());
  this->recv_msg_ok_ = 1;

  int response_code = 200;
  // get range
  int64_t begin_pos = 0;
  int64_t end_pos = -1;
  int ret = this->get_range(http_header, begin_pos, end_pos);
  if (ret == 206)
  { 
    response_code = 206;
    this->session_desc_ = "206 Partial Content";
  }else if (ret != 200)
  {
    this->response_to_client(ret);
    return -1;
  }else
    this->session_desc_ = "200 OK";

  this->session_id_ = push_sessionmgr::instance()->alloc_sessionid();
  push_session_ptr push_ss(new push_session(this->session_id_));
  push_ss->client(this);
  push_ss->request_url(url);
  push_ss->output_bdwidth_limit(bandwidth);
  push_ss->range_begin_pos(begin_pos);
  push_ss->range_end_pos(end_pos);
  push_sessionmgr::instance()->insert(push_ss);

  file_info_ptr fileinfo = file_manager::instance()->find(url);
  if (!fileinfo)  
  {
    handle_pull_file(url, url, this->session_id_, begin_pos, end_pos);
    return 0;
  }

  // client cache
  if ((fileinfo->status() & file_info::ST_COMPLETED)
      // must completed
      && (this->check_modified(http_header, 
                               fileinfo->mtime(),
                               fileinfo->etag())) == -1)
  {
    this->session_desc_ = "Not Modified!";
    this->response_to_client(304);
    push_sessionmgr::instance()->remove(this->session_id_);
    return -1;
  }

  // find file information.
  if (end_pos == -1)
    end_pos = fileinfo->length() - 1;

  this->content_length_ = end_pos - begin_pos + 1;
  push_ss->content_length(this->content_length_);

  if (this->http_method_ == HTTP_HEAD)
  {
    this->session_desc_ = "Get Resource Info!";
    this->response_to_client(response_code, 
                             begin_pos, 
                             end_pos, 
                             this->content_length_,
                             fileinfo->mtime());
    push_sessionmgr::instance()->remove(this->session_id_);
    return -1;
  }

  if (fileinfo->status() & file_info::ST_UNCOMPLETED)
  {
    this->session_desc_ = "File Is Uncompleted!";
    this->response_to_client(501);
    push_sessionmgr::instance()->remove(this->session_id_);
    return -1;
  }

  if (this->content_length_ == 0 && response_code == 206)
  {
    this->session_desc_ = "Range Is Empty!";
    this->response_to_client(response_code,
                             begin_pos, 
                             end_pos, 
                             fileinfo->length(),
                             fileinfo->mtime());
    push_sessionmgr::instance()->remove(this->session_id_);
    return -1;
  }

  // select transfer agent.
  transfer_agent *trans_agent = 0;
  if (fileinfo->length() <= (int64_t)g_max_mem_cache_size 
      && fileinfo->length() >= (int64_t)g_min_mem_cache_size
     )
  {
    ++g_cache_file_requests;
    trans_agent = new mem_cache_transfer(begin_pos, 
                                         this->content_length_);
    if (trans_agent->open(fileinfo) != 0)
    {
      // cache manager is full.
      delete trans_agent;
      trans_agent = 0;
    }else
    {
      this->session_desc_ = HIT_CACHE;
      ++g_hit_cache;
    }
  }

  if (trans_agent == 0)
  {
    // direct read file from disk.
    ++g_disk_file_reqeusts;
    trans_agent = new serial_file_transfer(begin_pos, 
                                           this->content_length_);
    if (trans_agent->open(fileinfo) != 0)
    {
      delete trans_agent;
      trans_agent = 0;
      this->session_desc_ = "Open File Failed!";
      this->response_to_client(503);
      push_sessionmgr::instance()->remove(this->session_id_);
      return -1;
    }else
    {
      this->session_desc_ = HIT_DISK;
      ++g_hit_disk;
    }
  }

  // response to client.
  result = this->response_to_client(response_code, 
                                    begin_pos, 
                                    end_pos, 
                                    fileinfo->length(),
                                    fileinfo->mtime());
  if (result != 0)
  {
    if (trans_agent != 0)
      delete trans_agent;
    this->session_desc_ = "Response To Client Failed!";
    push_sessionmgr::instance()->remove(this->session_id_);
    return -1;
  }
  result = handle_start_transfer_data(trans_agent,
                                      this->session_id_,
                                      bandwidth,
                                      begin_pos, 
                                      end_pos,
                                      this->content_length_,
                                      this);
  if (result != 0)
  {
    this->session_desc_ = my_strerr(-result);
    push_sessionmgr::instance()->remove(this->session_id_);
    return -1;
  }

  //
  net_log->debug("insert sessionid %d h = %d", 
                 this->session_id_,
                 this->get_handle());
  return 0;
}
int http_client::response_to_client(int status,
                                    int64_t begin_pos/* = 0*/,
                                    int64_t end_pos/* = 0*/,
                                    int64_t file_size/* = 0*/,
                                    time_t last_modified_time)
{
  this->response_code_ = status;
  std::ostringstream os;
  // header
  switch(status)
  {
  case 200:
    os << "HTTP/1.1 200 OK\r\n";
    break;
  case 206:
    os << "HTTP/1.1 206 Partial Content\r\n";
    break;
  case 304:
    os << "HTTP/1.1 304 Not Modified\r\n";
    break;
  case 400:
    os << "HTTP/1.1 400 Bad Request\r\n";
    break;
  case 403:
    os << "HTTP/1.1 403 Forbidden\r\n";
    break;
  case 404:
    os << "HTTP/1.1 404 Not Found\r\n";
    break;
  case 408:
    os << "HTTP/1.1 408 Request Timeout\r\n";
    break;
  case 413:
    os << "HTTP/1.1 413 Request Entity Too Large\r\n";
    break;
  case 416:
    os << "HTTP/1.1 416 Requested range not satisfiable\r\n";
    break;
  case 501:
    os << "HTTP/1.1 501 Not Implemented\r\n";
    break;
  case 503:
    os << "HTTP/1.1 503 Service Unavailable\r\n";
    break;
  default:
    assert(0);
    break;
  }

  os << "Server: fcache\r\n";

  // 
  if (status == 200 || status == 206)
  {
    char date[64] = {0};
    this->gmttime_to_str(::time(0), date, sizeof(date));

    char last_modified[64] = {0};
    this->gmttime_to_str(last_modified_time, 
                         last_modified, 
                         sizeof(last_modified));

    if (this->content_length_ == 0)
      this->content_length_ = end_pos - begin_pos + 1;
    os << "Date: " << date << "\r\n"
      << "Last-Modified: " << last_modified << "\r\n"
      << "Connection: close\r\n"
      << "Content-Length: " << end_pos - begin_pos + 1 << "\r\n";
    if (status == 206)
    {
      os << "Content-Range: bytes " 
        << begin_pos 
        << "-" 
        << end_pos 
        << "/" 
        << file_size
        << "\r\n";
    }
  }else
  {
    os << "Connection: close\r\n";
  }
  os << "\r\n";
  int result = this->peer().send(os.str().c_str(), os.str().length());
  if (result < 0)
  {
    net_log->error("send response failed! [h = %d][%s]",
                   this->get_handle(),
                   strerror(errno));
  }
  return result < 0 ? -1 : 0;
}
int http_client::get_range(const char *str,
                           int64_t &begin_pos,
                           int64_t &end_pos)
{
  // Range: bytes=0-499
  char *range_end_p = 0;
  char *range_p = http_parser::get_value(str,
                                         "Range",
                                         sizeof("Range") - 1,
                                         &range_end_p);
  if (range_p == 0)
    return 200;

  char *value_ptr = range_p + ::strspn(range_p, "bytes= \t");

  char *xx = value_ptr;
  while (*xx && xx < range_end_p)
  {
    if (*xx == ',') break;
    ++xx;
  }
  if (xx != range_end_p)
  {
    this->session_desc_ = "Not Support Multi Range.";
    return 501;
  }

  if (isdigit(*value_ptr))
  {
    begin_pos = ::strtoll(value_ptr, 0, 10);
    char *sep = ::strchr(value_ptr + 1, '-');
    if (sep != 0 && isdigit(*(sep + 1)))  // 123456-123
      end_pos = ::strtoll(sep + 1, 0, 10);
  }else
  {
    begin_pos = 0;
    if (isdigit(*(value_ptr + 1)))
      end_pos = ::strtoll(value_ptr + 1, 0, 10);
    else
    {
      this->session_desc_ = "Requested Range Not Satisfiable";
      return 416;
    }
  }
  return 206;
}
int http_client::check_modified(const char *http_header,
                                const time_t last_modified_time,
                                const std::string &etag)
{
  char *if_modified_since_end_p = 0;
  char *if_modified_since_p = http_parser::get_value(http_header,
                                                     "If-Modified-Since",
                                                     sizeof("If-Modified-Since") - 1,
                                                     &if_modified_since_end_p);
  if (if_modified_since_p == 0) return 0;
  std::string if_modified_time(if_modified_since_p, if_modified_since_end_p);
  struct tm gmt_time;
  if (strptime(if_modified_time.c_str(), 
               "%a, %d %b %Y %H:%M:%S GMT",
               &gmt_time) != 0)
    return 0;

  time_t t = mktime(&gmt_time);
  if (last_modified_time > t)
    return 0;
  else if (last_modified_time == t && !etag.empty())
  {
    char *etag_end_p = 0;
    char *etag_p = http_parser::get_value(http_header,
                                          "Etag",
                                          4,
                                          &etag_end_p);
    if (etag_p == 0) return 0;
    std::string et(etag_p, etag_end_p);
    if (et != etag)
      return 0;
  }
  return -1;
}
int http_client::parse_params(const char *param_p,
                              int &bandwidth,
                              std::string &url)
{
  // bandwidth
  char *bw_p = ::strstr(param_p, "bw=");
  if (bw_p == 0 
      || (*(bw_p - 1) != '?' && *(bw_p - 1) != '&') // 
      )
    return -1;
  bandwidth = ::atoi(bw_p + sizeof("bw=") - 1);

  // url
  char *url_p = ::strstr(param_p, "url=");
  if (url_p == 0 
      || (*(url_p - 1) != '?' && *(url_p - 1) != '&') // 
      )
    return -1;
  size_t len = ::strcspn(url_p + sizeof("url=") - 1, "&");
  url.assign(url_p + sizeof("url=") - 1, len);
  return 0;
}
// format time to "Fri Jul 23 17:37:27 CST 2010"
char *http_client::gmttime_to_str(time_t now, char *str, size_t len)
{
  if (len < sizeof("Fri, Jul 23 17:37:27 CST 2010")) return str;
  struct tm t;
  if (now == 0) now = ::time(0);
  ::gmtime_r(&now, &t);
  ::strftime(str, len, "%a, %d %b %Y %H:%M:%S GMT", &t);
  if (len > sizeof("Fri, Jul 23 17:37:27 CST 2010")) 
    len = sizeof("Fri, Jul 23 17:37:27 CST 2010");
  str[len-1] = '\0';
  return str;
}
void http_client::show_status()
{
  std::string msg;
  //service_status::instance()->show_status(msg);
  std::ostringstream os;
  os << "HTTP/1.0 200 OK\r\n"
    << "Server: fcached\r\n"
    << "Accept-Ranges: bytes\r\n"
    << "Connection: close\r\n"
    << "Content-Length: " << msg.length() << "\r\n"
    << "\r\n"
    << msg;
  this->peer().send(msg.c_str(), msg.length());
}
