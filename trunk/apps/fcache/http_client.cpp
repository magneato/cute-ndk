#include "http_client.h"
#include "http_parser.h"
#include "http_session.h"
#include "file_manager.h"
#include "buffer_manager.h"
#include "http_sessionmgr.h"
#include "dispatch_data_task.h"
#include "mem_cache_transfer.h"
#include "serial_file_transfer.h"

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
  int len = ::snprintf(log, sizeof(log),
                       "\"%s.%03lu\" \"%s.%03lu\" %s %s %s %d %ld %ld \"%s\"\n",
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
#else
  int len = ::snprintf(log, sizeof(log),
                       "\"%s.%03lu\" \"%s.%03lu\" %s %s %s %d %lld %lld \"%s\"\n",
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
#endif

  access_log->puts(log, len);
}
int http_client::open(void *)
{
  char addr[32] = {0};  // 
  this->remote_addr_.addr_to_string(addr, sizeof(addr));
  net_log->debug("new connection [%s][%d]", addr, this->peer().get_handle());
  this->recv_buff_ = new ndk::message_block(1024*2);
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
                                 this->recv_buff_->space());
  if (result < 0)
  {
    if (errno == EWOULDBLOCK)
      return 0;
    net_log->error("socket %d error [%s]!",
                   this->peer().get_handle(),
                   strerror(errno));
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

  if (this->recv_buff_->space() == 0)
  {
    ndk::message_block *mb = 
      new ndk::message_block(this->recv_buff_->size() * 2);
    std::memcpy(mb->data(), 
                this->recv_buff_->data(), 
                this->recv_buff_->length());
    this->recv_buff_->release();
    this->recv_buff_ = mb;
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
static const char *hit_cache = "HIT CACHE";
static const char *hit_disk = "HIT DISK";
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
  // check method
  if (ptr[0] == 'G' && ptr[1] == 'E' && ptr[2] == 'T')
    this->http_method_ = HTTP_GET;
  else if (ptr[0] == 'H' && ptr[1] == 'E' && ptr[2] == 'A' && ptr[3] == 'D')
    this->http_method_ = HTTP_HEAD;
  else
  {
    net_log->error("unsupport http method [%s]", ptr);
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
    this->session_desc_ = "Parse uri failed";
    this->response_client(400);
    return -1;
  }
  this->uri_.assign(uri_begin_p, uri_end_p);

  // get host
  char *host_p_end = 0;
  char *host_p = http_parser::get_value(uri_end_p,
                                        "Host",
                                        4,
                                        &host_p_end);
  if (host_p == 0)
  {
    this->session_desc_ = "Not found 'Host' header";
    this->response_client(400);
    return -1;
  }
  std::string host(host_p, host_p_end);
  if (host.find(':') == std::string::npos)
  {
    std::ostringstream ostr;
    ostr << host << ":" << g_listen_port;
    host = ostr.str();
  }

  // get range
  int64_t begin_pos = 0;
  int64_t end_pos = -1;
  int partial = 0;
  int ret = this->get_range(uri_end_p, begin_pos, end_pos);
  if (ret == 206)
    partial = 1;
  else if (ret != 200)
  {
    this->response_client(ret);
    return -1;
  }

  // interface.
  if (this->uri_.length() == 1)
  {
    this->show_status();
    return -1;
  }
  std::string url = "http://" + host + this->uri_;

  std::string filename = g_doc_root + this->uri_;

  fileinfo_ptr fileinfo = file_manager::instance()->find(url);
  if (!fileinfo)  
  {
    struct stat st;
    if (::stat(filename.c_str(), &st) != -1)
    {
      if ((st.st_mode & S_IFMT) == S_IFREG)
      {
        fileinfo = fileinfo_ptr(new file_info());
        fileinfo->length(st.st_size);
        fileinfo->url(url);
        fileinfo->filename(filename);
        fileinfo->mtime(st.st_mtime);
        file_manager::instance()->insert(fileinfo);
      }else
      {
        this->session_desc_ = "Not a regular file";
        this->response_client(403);
        return -1;
      }
    }else
    {
      this->session_desc_ = strerror(errno);
      if (errno == ENOENT)
        this->response_client(404);
      else
        this->response_client(503);
      return -1;
    }
  }

  this->recv_msg_ok_ = 1;
  ++g_requests;
  if (g_payload > g_concurrency_num)
  {
    this->session_desc_ = "Out of payload";
    this->response_client(503);
    return -1;
  }

  if (end_pos == -1)
    end_pos = fileinfo->length() - 1;

  this->content_length_ = end_pos - begin_pos + 1;

  if (this->http_method_ == HTTP_HEAD)
  {
    this->session_desc_ = "Get resource infor!";
    this->response_client(200, begin_pos, end_pos);
    return -1;
  }

  int result = 0;
  transfer_agent *trans_agent = 0;
  if (fileinfo->length() <= (int64_t)g_max_mem_cache_size 
      && fileinfo->length() >= (int64_t)g_min_mem_cache_size)
  {
    ++g_cache_file_requests;
    trans_agent = new mem_cache_transfer(begin_pos, this->content_length_);
    result = trans_agent->open(fileinfo);
    if (result != 0)
    {
      // cache manager is full.
      delete trans_agent;
      trans_agent = 0;
    }else
    {
      this->session_desc_ = hit_cache;
    }
  }

  // direct read file from disk.
  if (trans_agent == 0)
  {
    ++g_disk_file_reqeusts;
    trans_agent = new serial_file_transfer(begin_pos, this->content_length_);
    if (trans_agent->open(fileinfo) != 0)
    {
      delete trans_agent;
      this->response_client(503);
      return -1;
    }else
    {
      this->session_desc_ = hit_disk;
      ++g_hit_disk;
    }
  }
  if (partial == 1)
    result = this->response_client(206, begin_pos, end_pos, fileinfo->length());
  else
    result = this->response_client(200, begin_pos, end_pos);
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
    job->content_length_ = this->content_length_;
    g_dispatch_data_task[0].push_job(job);
    net_log->debug("insert sessionid %d h = %d", 
                   this->session_id_,
                   this->get_handle());
    ++g_payload;
  }else if (trans_agent) 
  {
    delete trans_agent;
  }
  return result;
}
int http_client::response_client(int status,
                                 int64_t begin_pos/* = 0*/,
                                 int64_t end_pos/* = 0*/,
                                 int64_t file_size/* = 0*/)
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
  case 400:
    os << "HTTP/1.1 400 Bad Request\r\n";
    break;
  case 403:
    os << "HTTP/1.1 403 Forbidden\r\n";
    break;
  case 404:
    os << "HTTP/1.1 404 Not Found\r\n";
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
    os << "Accept-Ranges: bytes\r\n"
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

  net_log->error("%s", value_ptr);
  char *xx = value_ptr;
  while (*xx && xx < range_end_p)
  {
    if (*xx == ',') break;
    ++xx;
  }
  if (xx != range_end_p)
  {
    this->session_desc_ = "Not support multi Range.";
    return 501;
  }

  char *endptr = 0;
  if (isdigit(*value_ptr))
  {
    begin_pos = ::strtoll(value_ptr, &endptr, 10);
    char *sep = ::strchr(value_ptr + 1, '-');
    if (sep != 0 && isdigit(*(sep + 1)))  // 123456-123
      end_pos = ::strtoll(sep + 1, &endptr, 10);
  }else
  {
    begin_pos = 0;
    if (isdigit(*(value_ptr + 1)))
      end_pos = ::strtoll(value_ptr + 1, &endptr, 10);
    else
    {
      this->session_desc_ = "Requested range not satisfiable";
      return 416;
    }
  }
  return 206;
}
int http_client::show_status()
{
  //g_cache_manager->check();  // for debug
  STRACE("");
  static char requests[]     = "requests";
  static int  requests_w     = sizeof(requests) + 1;
  static char c_file_reqs[]  = "c_file_reqs";
  static int  c_file_reqs_w  = sizeof(c_file_reqs) + 1;
  static char d_file_reqs[]  = "d_file_reqs";
  static int  d_file_reqs_w  = sizeof(d_file_reqs) + 1;
  static char finished[]     = "finished";
  static int  finished_w     = sizeof(finished) + 1;
  static char hit_cache[]    = "hit_cache";
  static int  hit_cache_w    = sizeof(hit_cache) + 1;
  static char hit_disk[]     = "hit_disk";
  static int  hit_disk_w     = sizeof(hit_disk) + 1;
  static char hit_c_rate[]   = "hit_cache_rate";
  static int  hit_c_rate_w   = sizeof(hit_c_rate) + 1;
  static char payload[]      = "payload";
  static int  payload_w      = sizeof(payload) + 1;
  static char cache_mem_used[]  = "c_mem_used(mb)";
  static int  cache_mem_used_w  = sizeof(cache_mem_used) + 1;
  static char aio_mem_used[]  = "aio_mem_used(mb)";
  static int  aio_mem_used_w  = sizeof(aio_mem_used) + 1;

  std::ostringstream  ostr;
  ostr << std::setfill(' ')
    << std::setiosflags(std::ios::left);
  ostr << std::setw(requests_w) << requests
    << std::setw(finished_w) << finished
    << std::setw(c_file_reqs_w) << c_file_reqs 
    << std::setw(d_file_reqs_w) << d_file_reqs 
    << std::setw(hit_cache_w) << hit_cache
    << std::setw(hit_c_rate_w) << hit_c_rate
    << std::setw(hit_disk_w) << hit_disk
    << std::setw(payload_w) << payload
    << std::setw(cache_mem_used_w) << cache_mem_used
    << std::setw(aio_mem_used_w) << aio_mem_used
    << std::endl;
  int h_c_rate  = 0;
  if (g_finished > 0 && g_hit_cache > 0)
    h_c_rate = int((g_hit_cache * 100) / g_cache_file_requests);
  ostr << std::setw(requests_w) << g_requests
    << std::setw(finished_w) << g_finished
    << std::setw(c_file_reqs_w) << g_cache_file_requests
    << std::setw(d_file_reqs_w) << g_disk_file_reqeusts
    << std::setw(hit_cache_w) << g_hit_cache
    << std::setw(hit_c_rate_w) << h_c_rate
    << std::setw(hit_disk_w) << g_hit_disk
    << std::setw(payload_w) << g_payload
    << std::setw(cache_mem_used_w) << (g_cache_mem_used > 0 ? 
    g_cache_mem_used/1024/1024 : g_cache_mem_used)
    << std::setw(aio_mem_used_w) << file_io_cache_mgr->alloc_blocks()
    << std::endl << std::endl;
#if 0
  std::deque<std::string> l = file_manager::instance()->get_all_urls();
  std::deque<std::string>::iterator lp;
  for (lp = l.begin(); lp != l.end(); ++lp)
    ostr << *lp << std::endl;
#endif

  std::ostringstream os;
  os << "HTTP/1.0 200 OK\r\n"
    << "Server: fcache\r\n"
    << "Accept-Ranges: bytes\r\n"
    << "Connection: close\r\n"
    << "Content-Length: " << ostr.str().length() << "\r\n"
    << "\r\n"
    << ostr.str().c_str();
  this->peer().send(os.str().c_str(), os.str().length());
  return -1;
}
