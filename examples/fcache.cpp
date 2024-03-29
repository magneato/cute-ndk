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
#include <ndk/atomic_opt.h>
#include <ndk/svc_handler.h>
#include <ndk/message_block.h>
#include <ndk/acceptor.h>
#include <ndk/task.h>
#include <ndk/logger.h>
#include <ndk/epoll_reactor.h>
#include <ndk/select_reactor.h>
#include <ndk/mem_pool.h>
#include <ndk/cache_manager.h>

#define TIME_PIECE  50  //msec

static ndk::logger *net_log = ndk::log_manager::instance()->get_logger("root.fcache_test");

// global vars
int g_requests       = 0;
int g_request_ok     = 0;
int g_finished       = 0;
int g_hit            = 0;
int g_miss           = 0;
int g_payload        = 0;
int g_put_obj        = 0;
int g_drop_obj       = 0;

int concurrent_number = 100;
int bandwidth_limit   = 2048;  // Kbps
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

class http_client;
class http_session
{
public:
  http_session(int sid)
    : session_id_(sid),
    output_bandwidth_(0),
    client_(0)
  { http_session::count++; }

  ~http_session()
  { http_session::count--; }

  inline void session_id(int sid)
  { this->session_id_ = sid; }

  inline int session_id()
  { return this->session_id_; }

  inline void output_bandwidth(int bw)
  { this->output_bandwidth_ = bw; }

  inline int output_bandwidth()
  { return this->output_bandwidth_; }

  inline void client(http_client *c)
  { this->client_ = c; }

  inline http_client *client(void)
  { return this->client_; }

public:
  static int count;
private:
  int session_id_;
  int output_bandwidth_;
  http_client *client_;
};
int http_session::count = 0;
/**
 * @brief http session mgr
 */
class http_sessionmgr : public ndk::singleton<http_sessionmgr>
{
public:
  http_sessionmgr()
  : session_ids_(0)
  { }

  // return session id.
  int insert(http_session *hs)
  {
    std::pair<session_list_itor, bool> ret = 
      this->session_list_.insert(std::make_pair(hs->session_id(),
                                                hs));
    assert(ret.second == true);
    return ret.second ? 0 : -1;
  }

  http_session *find(int sid)
  {
    if (sid < 0) return 0;
    session_list_itor itor = this->session_list_.find(sid);
    if (itor == this->session_list_.end())
      return 0;
    return itor->second;
  }

  http_session *remove(int sid)
  {
    session_list_itor itor = this->session_list_.find(sid);
    if (itor == this->session_list_.end())
      return 0;
    http_session *hs = itor->second;
    this->session_list_.erase(itor);
    return hs;
  }

  int alloc_sessionid()
  {
    return this->session_ids_++;
  }

  size_t size()
  {
    return this->session_list_.size();
  }
private:
  int session_ids_;

  typedef std::map<int, http_session *> session_list_t;
  typedef std::map<int, http_session *>::iterator session_list_itor;
  session_list_t session_list_;
};
/**
 * @brief diskpatch data task
 */
class dispatch_data_task : public ndk::task, public ndk::singleton<dispatch_data_task>
{
public:
  class dispatch_job
  {
  public:
    dispatch_job()
      : stopped(0),
      session_id(-1),
      bytes_to_send_per_timep(0),
      client(0)
    { dispatch_job::count++; }

    ~dispatch_job()
    { dispatch_job::count--; }
    
  public:
    static int count;
    int stopped;
    int session_id;
    int bytes_to_send_per_timep;
    http_client *client;
    ndk::time_value last_check_bandwidth_time;
  };
public:
  dispatch_data_task()
  : task_idle_(1),
  dispatch_queue_not_empty_cond_(dispatch_queue_mtx_)
  { }

  int open()
  {
    STRACE("");
    return this->activate(ndk::thread::thr_join, 1);
  }

  virtual int svc()
  {
    STRACE("");
    while (1)
    {
      if (this->task_idle_)
      {
        ndk::guard<ndk::thread_mutex> g(this->dispatch_queue_mtx_);
        this->dispatch_queue_not_empty_cond_.wait();
      }

      this->dispatch_data();
    }
    return 0;
  }
  int out_of_bandwidth(dispatch_job *job, ndk::time_value &now)
  {
    STRACE("");
    ndk::time_value diff_time = now - job->last_check_bandwidth_time;
    if ((diff_time.msec() > 1000))
    {
      job->last_check_bandwidth_time = now;
      job->bytes_to_send_per_timep = 0;
      return 1;
    }
    return 0;
  }
  void push_job(dispatch_job *job)
  {
    STRACE("");
    assert(job);
    ndk::guard<ndk::thread_mutex> g(this->dispatch_queue_mtx_);
    this->dispatch_queue_.push_back(job);
    job->last_check_bandwidth_time.update();
    dispatch_queue_not_empty_cond_.signal();
  }
  void delete_client(int sid)
  {
    STRACE("");
    ndk::guard<ndk::thread_mutex> g(this->dispatch_queue_mtx_);
    dispatch_queue_itor itor = this->dispatch_queue_.begin();
    for (; itor != this->dispatch_queue_.end(); ++itor)
    {
      if ((*itor)->session_id == sid)
      {
        delete *itor;
        this->dispatch_queue_.erase(itor);
        break;
      }
    }
  }
protected:
  void dispatch_data(void);

  void delete_job_i(dispatch_job *job);
private:
  int task_idle_;
  ndk::time_value poll_before_;
  ndk::time_value poll_after_;
  ndk::time_value diff_time_;
  typedef std::deque<dispatch_job *> dispatch_queue_t;
  typedef std::deque<dispatch_job *>::iterator dispatch_queue_itor;
  dispatch_queue_t dispatch_queue_;
  ndk::thread_mutex dispatch_queue_mtx_;
  ndk::condition<ndk::thread_mutex> dispatch_queue_not_empty_cond_;
};
int dispatch_data_task::dispatch_job::count = 0;
/**
 * @brief http client
 */
class http_client : public ndk::svc_handler
{
public:
  http_client()
    : session_id_(-1),
    recv_msg_ok_(0),
    send_bytes_(0),
    bytes_to_send_(4096),
    content_length_(0),
    timer_id_(-1),
    file_handle_(NDK_INVALID_HANDLE),
    cache_obj_(0),
    recv_buff_(0)
  { http_client::count++; }

  virtual ~http_client()
  {
    STRACE("");
    http_client::count--;
    if (this->session_id_ != -1)
    {
      dispatch_data_task::instance()->delete_client(this->session_id_);
      http_session *hs = 
        http_sessionmgr::instance()->remove(this->session_id_);
      if (hs) delete hs;
      net_log->debug("remove sessionid %d", this->session_id_);
      this->session_id_ = -1;
    }
    if (this->recv_buff_)
    {
      this->recv_buff_->release();
      http_client::msg_count--;
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
      this->cache_obj_ = 0;
    }
    if (this->content_length_ > 0 && this->send_bytes_ == this->content_length_)
    {
      ++g_finished;
      ndk::time_value diff = ndk::time_value::gettimeofday() - this->begin_time_;
      net_log->rinfo("send data completely! [%d bytes used %lu msecs]", 
                     this->content_length_,
                     diff.msec());
    }
    if (this->recv_msg_ok_)
      --g_payload;
  }
  inline virtual void set_remote_addr(const ndk::inet_addr &remote_addr)
  { this->remote_addr_ = remote_addr; }

  virtual int open(void *arg)
  {
    STRACE("");
    char addr[32] = {0};  // 
    this->remote_addr_.addr_to_string(addr, sizeof(addr));
    net_log->debug("new connection [%s][%d]", addr, this->peer().get_handle());
    this->recv_buff_ = new ndk::message_block(1024*1024);
    http_client::msg_count++;
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
    if (this->recv_msg_ok_)
    {
      net_log->error("handle %d had recv a msg!",
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
           || m == ndk::event_handler::write_mask
           || m == ndk::event_handler::all_events_mask);
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
    {
      net_log->error("error http request [%s]",
                     ptr);
      return this->response_client(-1);
    }
    std::string uri(uri_begin_p, uri_end_p - uri_begin_p);
    if (uri.empty() || uri[0] == '?')
      return this->show_status();

    if (g_payload > concurrent_number)
      return this->response_client(-2);

    this->recv_msg_ok_ = 1;


    ++g_payload;
    ++g_requests;

    uri = work_path + "/" + uri;
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
        if (data == 0 || g_cache_manager->put(uri, data, st.st_size, 
                                              g_cache_object_observer,
                                              this->cache_obj_) != 0)
        {
          result = st.st_size;
          this->recv_buff_->reset();
          if (data)
          {
            net_log->rinfo("put cache to manager failed!");
            ::munmap(data, st.st_size);
            assert(this->cache_obj_ == 0);  // for debug
          }else
            net_log->error("mmap failed![%s]", strerror(errno));
        }else
        {
          net_log->rinfo("hit [%s] failed! [refcount = %d][cobj = %p]", 
                         uri.c_str(),
                         this->cache_obj_->refcount(),
                         this->cache_obj_);
          assert(this->cache_obj_ != 0);  // for debug
          ++g_put_obj;
          result = st.st_size;
          if (this->file_handle_ != -1) 
            ::close(this->file_handle_);
          this->file_handle_ = NDK_INVALID_HANDLE;
        }
      }else 
      {
        net_log->error("open failed! [%s]", strerror(errno));
        result = -1;
      }
    }else
    {
      net_log->rinfo("hit [%s] ok!", uri.c_str());
      result = this->cache_obj_->size();
      ++g_hit;
    }
    if (result != -1)
    {
#ifndef MULTI_THREAD 
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
#else
      this->response_client(result);
      this->session_id_ = http_sessionmgr::instance()->alloc_sessionid();
      http_session *hs = new http_session(this->session_id_);
      hs->client(this);
      http_sessionmgr::instance()->insert(hs);
      dispatch_data_task::dispatch_job *job = 
        new dispatch_data_task::dispatch_job;
      job->session_id = this->session_id_;
      job->client = this;
      dispatch_data_task::instance()->push_job(job);
      net_log->debug("push a job [sid = %d]", this->session_id_);
#endif
      ++g_request_ok;
#ifdef MULTI_THREAD
      return 0;
#endif
    }
    return this->response_client(result);
  }
  int response_client(int result)
  {
    STRACE("");
    std::ostringstream os;
    if (result == -1)
      os << "HTTP/1.1 404 Not Found\r\n";
    else if (result == -2)
      os << "HTTP/1.1 503 Service Unavailable\r\n";
    else
      os << "HTTP/1.1 200 OK\r\n";
    os << "Server: fcache\r\n"
      << "Accept-Ranges: bytes\r\n"
      << "Connection: keep-alive\r\n";
    if (result > 0)
    {
      os << "Content-Length: " << result << "\r\n";
      this->content_length_ = result;
      if (this->bytes_to_send_ > this->content_length_)
        this->bytes_to_send_ = this->content_length_;
    }
    os << "\r\n";
    this->peer().send(os.str().c_str(), os.str().length());
    return result;
  }
  int show_status()
  {
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
  virtual int handle_output(ndk::ndk_handle )
  {
    STRACE("");
    return this->send_data() < 0 ? -1 : 0;
  }
  int send_data(void)
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
      if (result == -2) return -2;
#ifdef NDEBUG
      if (errno != EWOULDBLOCK)
      {
        net_log->error("send data to %d failed! [%s]",
                       this->get_handle(),
                       strerror(errno));
      }
#endif
      return -1;
    }
    this->send_bytes_ += result;
    if (this->send_bytes_ == this->content_length_)
    {
      return -2;
    }else if (this->file_handle_ == NDK_INVALID_HANDLE
              && (this->content_length_ - this->send_bytes_ 
                  < this->bytes_to_send_))
    {
      if (this->content_length_ - this->send_bytes_ <= 0)
      {
        net_log->error("bytes to send is %d, bytes_to_send_ = %d",
                       this->content_length_ - this->send_bytes_,
                       this->bytes_to_send_);
      }
      this->bytes_to_send_ = this->content_length_ - this->send_bytes_;
      assert(this->bytes_to_send_ > 0);
    }

    return result;
  }
  int transfic_from_file()
  {
    STRACE("");
    int result = 0;
    if (this->recv_buff_->length() == 0)
    {
      if (this->recv_buff_->space() == 0)
        this->recv_buff_->reset();
      result = ::read(this->file_handle_, 
                      this->recv_buff_->wr_ptr(), 
                      this->recv_buff_->space());
      if (result <= 0)
      {
        return -2;
      }
      this->recv_buff_->wr_ptr(result);
    }
    result = this->peer().send(this->recv_buff_->rd_ptr(),
                               this->recv_buff_->length());
    if (result <= 0)
      return -1;
    this->recv_buff_->rd_ptr(result);
    return result;
  }
public:
  static int count;
  static int msg_count;
protected:
  int session_id_;
  int recv_msg_ok_;
  int send_bytes_;
  int bytes_to_send_;
  int content_length_;
  int timer_id_;
  ndk::ndk_handle file_handle_;
  ndk::cache_object* cache_obj_;
  ndk::time_value begin_time_;
  ndk::message_block *recv_buff_;
  ndk::inet_addr remote_addr_;
};
int http_client::count = 0;
int http_client::msg_count = 0;
/**
 * @brief notify event class
 */
class notify_event
{
public:
  notify_event(int sid)
    : session_id(sid)
  { notify_event::count++; }

  ~notify_event()
  { notify_event::count--; }
public:
  int session_id;
  static ndk::atomic_opt<int, ndk::thread_mutex> count;
};
ndk::atomic_opt<int, ndk::thread_mutex> notify_event::count;

class reactor_event_handler : public ndk::event_handler, public ndk::singleton<reactor_event_handler>
{
public:
  reactor_event_handler()
  { }

  virtual int handle_msg(void *msg)
  {
    notify_event *event = reinterpret_cast<notify_event *>(msg);
    http_session *hs = http_sessionmgr::instance()->find(event->session_id);
    if (hs)
    {
      net_log->debug("reactor event handler find session: %d",
                     event->session_id);
      http_client *hc = hs->client();
      assert(hc != 0);
      if (hc) delete hc;
    }else
    {
      net_log->debug("reactor event handler not find session: %d",
                     event->session_id);
    }
    delete event;
    return 0;
  }
};
void dispatch_data_task::delete_job_i(dispatch_job *job)
{
  STRACE("");
  if (ndk::reactor::instance()->notify(reactor_event_handler::instance(),
                                       new notify_event(job->session_id)) != 0)
  {
    net_log->error("notify session %d failed [%s]",
                   job->session_id,
                   strerror(errno));
  }
}
void dispatch_data_task::dispatch_data()
{
  STRACE("");
  this->poll_before_.update();
  {
    //ndk::guard<ndk::thread_mutex> g(this->dispatch_queue_mtx_);
    this->dispatch_queue_mtx_.acquire();

    dispatch_queue_itor itor = this->dispatch_queue_.begin();
    int result = 0;
    int stopped_count = 0;
    for (; itor != this->dispatch_queue_.end(); )
    {
      dispatch_job *job = *itor;
      if (job->stopped)
      {
        ++stopped_count;
        net_log->debug("session %d stopped!", job->session_id);
        goto LOOP_END;
      }else if (this->out_of_bandwidth(job, this->poll_before_))
        goto LOOP_END;

      job->bytes_to_send_per_timep += (bandwidth_limit*1024/(1000/TIME_PIECE))/8;

      for (; job->bytes_to_send_per_timep > 0; )
      {
        result = job->client->send_data();
        if (result < 0)
        {
          if (result == -1 && errno == EWOULDBLOCK)
            break;
          else
          {
            job->stopped = 1;
            this->delete_job_i(job);
            goto LOOP_END;
          }
        }else
        {
          job->bytes_to_send_per_timep -= result;
        }
      }

LOOP_END:
      ++itor;
LOOP_DONE:
    ;
    } // end `for (; itor != this->dispatch_queue_.end(); ...'
    if (this->dispatch_queue_.empty()
        || 
        stopped_count == this->dispatch_queue_.size())
      this->task_idle_ = 1;
    else
      this->task_idle_ = 0;

    this->dispatch_queue_mtx_.release();
  }
  this->poll_after_.update();
  this->diff_time_ = this->poll_after_ - poll_before_;
  int msec = TIME_PIECE - this->diff_time_.msec();
  if (msec > 5)
  {
    ndk::sleep(ndk::time_value(0, (msec-3)*1000));
  }
}

ndk::acceptor<http_client> *g_acceptor = 0;

void print_usage()
{
  printf("Usage: fcache [OPTION...]\n\n");
  printf("  -c  concurrency       Number of multiple requests to perform at a time.\n"); 
  printf("  -p  port              Listen port(default is 8800)'\n");
  printf("  -d  path              Document root(default is current path)'\n");
  printf("  -b  kbps              Max of ouput bandwidth'\n");
  printf("  -h  MB                Cache manager high water mark'\n");
  printf("  -l  MB                Cache manager low water mark'\n");
  printf("\n");
  printf("You can use 'watch curl -s http://localhost:port/' to monitor me.\n\n");
}
void dump_info(int )
{
  //g_cache_manager->check();
  g_cache_manager->dump();
  g_cache_manager->flush_all();
  ndk::reactor::instance()->dump();
  NDK_LOG("notify event new/delete diff is %d", notify_event::count.value());
  NDK_LOG("http session count is %d", http_session::count);
  NDK_LOG("http session list size is %d", http_sessionmgr::instance()->size());
  NDK_LOG("http client count is %d", http_client::count);
  NDK_LOG("http client msg count is %d", http_client::msg_count);
  NDK_LOG("dispatch job count is %d", dispatch_data_task::dispatch_job::count);
}
int main(int argc, char *argv[])
{
  if (argc == 1)
  {
    print_usage();
    return 0;
  }
  signal(SIGPIPE, SIG_IGN);
  signal(SIGTRAP, SIG_IGN);
  signal(SIGHUP, dump_info);
  int c = -1;
  const char *opt = "c:p:d:b:h:l:";
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
    case 'b':
      bandwidth_limit = ::atoi(optarg);
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
#ifdef MULTI_THREAD
  ndk::epoll_reactor<ndk::reactor_token> *r_impl
    = new ndk::epoll_reactor<ndk::reactor_token>();
#else
  ndk::epoll_reactor<ndk::reactor_null_token> *r_impl
    = new ndk::epoll_reactor<ndk::reactor_null_token>();
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
  g_cache_manager = new ndk::cache_manager<std::string, ndk::null_mutex>(32,
                                                                         20*1024*1024,
                                                                         high_water,
                                                                         low_water);
#ifdef MULTI_THREAD
  dispatch_data_task::instance()->open();
#endif

  ndk::reactor::instance()->run_reactor_event_loop();
  net_log->error("reactor exit! [%s]", strerror(errno));
  return 0;
}

