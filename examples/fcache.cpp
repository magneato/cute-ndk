// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-21 21:57
 * Brief    : For testing <epoll_reactor>, <acceptor>.
 */
//========================================================================

#include <fstream>
#include <iomanip>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <cassert>

#include <ndk/thread_mutex.h>
#include <ndk/reactor.h>
#include <ndk/svc_handler.h>
#include <ndk/message_block.h>
#include <ndk/acceptor.h>
#include <ndk/task.h>
#include <ndk/logger.h>
#include <ndk/epoll_reactor.h>
#include <ndk/select_reactor.h>
#include <ndk/mem_pool.h>
#include <ndk/cache_manager.h>

static ndk::logger *net_log = ndk::log_manager::instance()->get_logger("root.fcache");

// global vars
int g_requests       = 0;
int g_finished       = 0;
int g_hit            = 0;
int g_miss           = 0;
int g_payload        = 0;
int g_put_obj        = 0;
int g_drop_obj       = 0;

int concurrent_number = 100;
int listen_port = 8800;
int high_water  = 1024;
int low_water   = 896;
std::string work_path = "./";

ndk::cache_manager<std::string, ndk::null_mutex> *g_cache_manager = 0;

class observer : public ndk::cache_object_observer
{
public:
  virtual int drop(ndk::cache_object *cobj)
  {
    ++g_drop_obj;
    net_log->rinfo("drop one cache object [%p]", cobj);
    ::munmap(cobj->data(), cobj->size());
    return 0;
  }
};
observer *g_cache_object_observer = new observer();
class http_client : public ndk::svc_handler
{
public:
  http_client()
    : recv_msg_ok_(0),
    send_bytes_(0),
    bytes_to_send_(8192),
    timer_id_(-1),
    file_handle_(NDK_INVALID_HANDLE),
    cache_obj_(0),
    recv_buff_(0)
  { }

  virtual ~http_client()
  {
    STRACE("");
    if (this->recv_buff_)
    {
      delete [](char *)this->recv_buff_->data();
      this->recv_buff_->release();
    }
    this->recv_buff_ = 0;

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
    this->cache_obj_ = 0;
  }
  virtual int open(void *arg)
  {
    STRACE("");
    ndk::inet_addr remote_addr;
    if (this->peer().get_remote_addr(remote_addr) != 0)
      net_log->error("get %d remote addr failed! [%s]",
                     this->get_handle(),
                     strerror(errno));
    char addr[32] = {0};  // 
    remote_addr.addr_to_string(addr, sizeof(addr));
    net_log->debug("new connection [%s][%p]", addr, this);
    char *p = new char[4096];
    this->recv_buff_ = new ndk::message_block(p, 4095);
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
    assert(this->timer_id_ != -1);
    return this->timer_id_ == -1 ? -1 : 0;
  }
  virtual int handle_input(ndk::ndk_handle )
  {
    STRACE("");
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

    this->recv_buff_->wr_ptr(result);

    return this->handle_data(this->recv_buff_);
  }
  virtual int handle_timeout(const void *, const ndk::time_value &now)
  {
    STRACE("");
    if (this->recv_msg_ok_)
      return 0;
    ndk::time_value tv = now - this->begin_time_;
    net_log->error("[%d] recv request timeout [escape %d.%d]",
                   this->peer().get_handle(),
                   tv.sec(),
                   (int)tv.msec());
    return -1;
  }
  virtual int handle_close(ndk::ndk_handle , ndk::reactor_mask m)
  {
    STRACE("");
    assert(m == ndk::event_handler::read_mask 
           || m == ndk::event_handler::timer_mask
           || m == ndk::event_handler::write_mask);
    if (this->recv_msg_ok_)
      --g_payload;
    this->destroy();
    return 0;
  }
  int handle_data(ndk::message_block *mb)
  {
    STRACE("");
    mb->set_eof();
    char *ptr = mb->data();
    char *p = ::strstr(ptr, "\r\n\r\n");
    int head_length = p + 4 - ptr;
    if (p == 0)
    {
      net_log->rinfo("recv an uncompletd request");
      return 0;
    }
    char *uri_begin_p = ptr + 4 + 1;
    char *uri_end_p = ::strstr(uri_begin_p, " HTTP/1.");
    if (uri_end_p == 0)
      return this->response_client(-1);
    std::string uri(uri_begin_p, uri_end_p - uri_begin_p);
    if (uri.empty() || uri[0] == '?')
      return this->show_status();

    this->recv_msg_ok_ = 1;

    if (g_payload > concurrent_number)
      return this->response_client(-1);

    ++g_payload;
    ++g_requests;

    uri = work_path + uri;
    int result = 0;
    this->cache_obj_ = g_cache_manager->get(uri);
    if (this->cache_obj_ == 0)
    {
      this->file_handle_ = ::open(uri.c_str(), O_RDONLY);
      struct stat st;
      if (this->file_handle_ != -1 && ::fstat(this->file_handle_, &st) != -1)
      {
        ++g_miss;
        void *data = ::mmap(0, st.st_size, PROT_READ, 
                            MAP_SHARED, this->file_handle_, 0);
        if (data && g_cache_manager->put(uri, data, st.st_size, 
                                         g_cache_object_observer,
                                         this->cache_obj_) != 0)
        {
          result = st.st_size;
          this->recv_buff_->reset();
          net_log->error("put cache to manager failed!");
        }else
        {
          net_log->rinfo("hit [%s] failed! [refcount = %d][cobj = %p]", 
                         uri.c_str(),
                         this->cache_obj_->refcount(),
                         this->cache_obj_);
          ++g_put_obj;
          result = st.st_size;
          if (this->file_handle_ != -1) 
            ::close(this->file_handle_);
          this->file_handle_ = NDK_INVALID_HANDLE;
        }
      }else 
        result = -1;
    }else
    {
      net_log->rinfo("hit [%s] ok!", uri.c_str());
      result = this->cache_obj_->size();
      ++g_hit;
    }
    if (result != -1)
    {
      if (this->get_reactor() 
          && this->get_reactor()->register_handler(this->peer().get_handle(),
                                                   this, 
                                                   event_handler::write_mask) == -1)
      {
        net_log->error("[%d] register writeable mask to reactor failed! [%s]", 
                       this->get_handle(),
                       strerror(errno));
        return -1;
      }
    }
    return this->response_client(result);
  }
  int response_client(int result)
  {
    STRACE("");
    std::ostringstream os;
    if (result == -1)
      os << "HTTP/1.1 404 Not Found\r\n";
    else
      os << "HTTP/1.1 200 OK\r\n";
    os << "Server: fcache\r\n"
      << "Accept-Ranges: bytes\r\n"
      << "Connection: keep-alive\r\n";
    if (result != -1)
    {
      os << "Content-Length: " << result << "\r\n";
      this->content_length_ = result;
    }
    os << "\r\n";
    this->peer().send(os.str().c_str(), os.str().length());
    return result;
  }
  int show_status()
  {
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
      hrate = g_hit * 100 / g_finished;
    int mrate = 0;
    if (g_finished > 0)
      mrate = g_miss * 100 / g_finished;
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
    os << "HTTP/1.1 200 OK\r\n"
      << "Server: fcache\r\n"
      << "Accept-Ranges: bytes\r\n"
      << "Connection: close\r\n"
      << "Content-Length: " << ostr.str().length() << "\r\n"
      << "\r\n"
      << ostr.str().c_str();
    this->peer().send(os.str().c_str(), os.str().length());
    return -1;
  }
  virtual int handle_output(ndk::ndk_handle h)
  {
    STRACE("");
    int result = 0;
    if (this->file_handle_ != NDK_INVALID_HANDLE)
    {
      result = this->transfic_from_file();
    }else
    {
      result = this->peer().send(((char *)this->cache_obj_->data()) 
                                 + this->send_bytes_,
                                 this->bytes_to_send_);
    }
    if (result < 0)
    {
      net_log->error("send data to %d failed! [%s]",
                     h,
                     strerror(errno));
      return -1;
    }
    this->send_bytes_ += result;
    if (this->send_bytes_ >= this->content_length_)
    {
      ++g_finished;
      ndk::time_value diff = ndk::time_value::gettimeofday() - this->begin_time_;
      net_log->rinfo("send data completely! [%d bytes used %lu msecs]", 
                     this->content_length_,
                     diff.msec());
      return -1;
    }else if (this->content_length_ - this->send_bytes_ 
              < this->bytes_to_send_)
    {
      this->bytes_to_send_ = this->content_length_ - this->send_bytes_;
    }

    return 0;
  }
  int transfic_from_file()
  {
    int result = ::read(this->file_handle_, 
                        this->recv_buff_->wr_ptr(), 
                        this->recv_buff_->space());
    if (result <= 0)
      return -1;
    this->recv_buff_->wr_ptr(result);
    result = this->peer().send(this->recv_buff_->rd_ptr(),
                               this->recv_buff_->length());
    if (result <= 0)
      return -1;
    this->send_bytes_ += result;
    this->recv_buff_->reset();
    return 0;
  }
protected:
  int recv_msg_ok_;
  int send_bytes_;
  int bytes_to_send_;
  int content_length_;
  int timer_id_;
  ndk::ndk_handle file_handle_;
  ndk::cache_object* cache_obj_;
  ndk::time_value begin_time_;
  ndk::message_block *recv_buff_;
};

ndk::acceptor<http_client> *g_acceptor = 0;

void print_usage()
{
  printf("Usage: fcache [OPTION...]\n\n");
  printf("  -c  concurrency       Number of multiple requests to perform at a time.\n"); 
  printf("  -p  port              Listen port(default is 8800)'\n");
  printf("  -d  path              Document root(default is current path)'\n");
  printf("  -h  MB                Cache manager high water mark'\n");
  printf("  -l  MB                Cache manager low water mark'\n");
}
int main(int argc, char *argv[])
{
  if (argc == 1)
  {
    print_usage();
    return 0;
  }
  signal(SIGPIPE, SIG_IGN);
  int c = -1;
  const char *opt = "c:p:d:h:l:";
  extern int optind, optopt;
  while ((c = getopt(argc, argv, opt)) != -1)
  {
    switch(c)
    {
    case 'c':
      concurrent_number = ::atoi(optarg);
      break;
    case 'p':
      listen_port = ::atoi(optarg);
      break;
    case 'd':
      work_path = optarg;
      break;
    case 'h':
      high_water = ::atoi(optarg);
      break;
    case 'l':
      low_water = ::atoi(optarg);
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
#if 1
  ndk::epoll_reactor<ndk::reactor_null_token> *r_impl
    = new ndk::epoll_reactor<ndk::reactor_null_token>();
#else
  ndk::select_reactor<ndk::reactor_null_token> *r_impl
    = new ndk::select_reactor<ndk::reactor_null_token>();
#endif
  if (r_impl->open() != 0)
  {
    net_log->error("open epoll reactor failed");
    return -1;
  }
  ndk::reactor::instance(new ndk::reactor(r_impl));
  g_acceptor = new ndk::acceptor<http_client>();
  ndk::inet_addr local_addr(listen_port);
  if (g_acceptor->open(local_addr, ndk::reactor::instance()) != 0)
  {
    net_log->error("open acceptor failed");
    return -1;
  }
  g_cache_manager = new ndk::cache_manager<std::string, ndk::null_mutex>(65535,
                                                                         1024,
                                                                         20*1024*1024,
                                                                         500,
                                                                         450);
  ndk::reactor::instance()->run_reactor_event_loop();
  net_log->error("reactor exit! [%s]", strerror(errno));
  return 0;
}

