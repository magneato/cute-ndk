// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-21 21:57
 * Brief    : For testing <epoll_reactor>, <acceptor>.
 */
//========================================================================

#include <deque>
#include <string>
#include <fstream>
#include <cassert>
#include <iomanip>
#include <signal.h>
#include <iostream>
#include <algorithm>

#include <ndk/reactor.h>
#include <ndk/svc_handler.h>
#include <ndk/message_block.h>
#include <ndk/acceptor.h>
#include <ndk/connector.h>
#include <ndk/task.h>
#include <ndk/logger.h>
#include <ndk/atomic_opt.h>
#include <ndk/epoll_reactor.h>
#include <ndk/select_reactor.h>

static ndk::logger *net_log = ndk::log_manager::instance()->get_logger("root.vodperf");

//
enum
{
  I_BIZ  = 0x11,
  I_SBS  = 0x12,
  I_SCS  = 0x13
};
enum
{
  svc_running = 0x01,
  svc_stop,
  svc_term,
  svc_exit  
};

int concurrent_number = 10;
int concurrent_number_keeping = 200;
int interval_period = 2;
int current_payload = 0;
int current_bandwidth = 0;
int start_interface = I_SCS;
int listen_port = 8800;
int multi_thread = 0;

//
int g_biz_req_total = 0;
int g_biz_req_succ = 0;
int g_biz_req_failed = 0;
int g_biz_con_failed = 0;
int g_biz_req_timeout = 0;
int g_sbs_req_total = 0;
int g_sbs_req_succ = 0;
int g_sbs_con_failed = 0;
int g_sbs_req_failed = 0;
int g_sbs_req_timeout = 0;
int g_scs_req_total = 0;
int g_scs_req_completed = 0;
int g_scs_con_failed = 0;
int g_scs_req_failed_404 = 0;
int g_scs_req_timeout = 0;
int g_scs_stream_timeout = 0;
long long int g_scs_flux_total = 0;
int g_scs_bandwidth = 0;
long long int g_period_flux = 0;

int svc_status = svc_running;

class assign_task_timer;
class  assign_task_task;
assign_task_timer *g_assign_task = 0;
assign_task_task *g_assign_task_task = 0;

namespace help
{
  std::string parse_hostname(const std::string &url)
  {
    size_t p1 = url.find("://");
    if (p1 != std::string::npos)
    {
      size_t p2 = url.find("/", p1 + 3);
      if (p2 != std::string::npos && p2 > (p1 + 3))
        return url.substr(p1 + 3, p2 - p1 - 3);
    }
    return "";
  }
}
static void clear_statistic_data()
{
  g_biz_req_total = 0;
  g_biz_req_succ = 0;
  g_biz_req_failed = 0;
  g_biz_con_failed = 0;
  g_biz_req_timeout = 0;
  g_sbs_req_total = 0;
  g_sbs_req_succ = 0;
  g_sbs_req_failed = 0;
  g_sbs_con_failed = 0;
  g_sbs_req_timeout = 0;
  g_scs_req_total = 0;
  g_scs_req_completed = 0;
  g_scs_con_failed = 0;
  g_scs_req_failed_404 = 0;
  g_scs_req_timeout = 0;
  g_scs_stream_timeout = 0;
  g_scs_flux_total = 0;
  g_scs_bandwidth = 0;
}
class interface;
ndk::connector<interface> *g_connector = 0;

class assign_task_task : public ndk::task
{
public:
  class job_item
  {
  public:
    job_item(interface *s, ndk::inet_addr &addr)
    : si(s),
    remote_addr(addr)
    { }

    interface *si;
    ndk::inet_addr remote_addr;
  };
  assign_task_task()
  { }

  int open(int thr_num = 5)
  {
    return this->activate(ndk::thread::thr_join, thr_num);
  }
  virtual int svc()
  {
    ndk::message_block *mb = 0;
    ndk::time_value timeout(2, 0);
    while (1)
    {
      mb = 0;
      this->getq(mb, &timeout);
      if (mb)
      {
        this->handle_job(mb);
        mb->release();
      }
    }
    return 0;
  }
  void handle_job(ndk::message_block *mb)
  {
    job_item *job = reinterpret_cast<job_item *>(mb->data());
    net_log->debug("get one job %p", job);
    ndk::time_value tout(3, 0);
    if (job)
      g_connector->connect(job->si, job->remote_addr, &tout);
    delete job;
  }
};
class interface : public ndk::svc_handler
{
public:
  interface(const std::string &fileid,
            const std::string &addr)
    : connect_ok_(0),
    recv_msg_ok_(0),
    reconnect_num_(0),
    begin_time_(0),
    fileid_(fileid),
    remote_addr_(addr),
    recv_buff_(0)
  { }
  virtual ~interface()
  {
    if (this->recv_buff_)
    {
      this->recv_buff_->release();
    }
    this->recv_buff_ = 0;
  }
  virtual int open(void *arg)
  {
    this->recv_buff_ = new ndk::message_block(4096);
    this->connect_ok_ = 1;
    this->begin_time_ = ::time(0);
    if (this->get_reactor() 
        && this->get_reactor()->register_handler(this, 
                                                 event_handler::read_mask) == -1)
      return -1;
    std::string msg;
    this->build_msg(msg);
    if (this->peer().send(msg.c_str(), msg.length()) < 0)
    {
      net_log->error("send msg to [%s] failed! [%s]"
                     , this->remote_addr_.c_str()
                     , strerror(errno));
      return -1;
    }
    return 0;
  }
  virtual int build_msg(std::string &msg)
  {
    std::ostringstream os;
    std::string::size_type p = this->fileid_.find("://");
    if (p == std::string::npos) return -1;
    p += 3;
    p = this->fileid_.find("/", p);
    if (p == std::string::npos) return -1;

    std::string uri = this->fileid_.substr(p);
    os << "GET " << uri << " HTTP/1.1" << "\r\n"
      << "Referer: http://www.keng.com" << "\r\n"
      << "User-Agent: ndk" << "\r\n"
      << "Connection: Keep-Alive" << "\r\n"
      << "Host: " << this->remote_addr_ << "\r\n"
      << "\r\n";
    msg = os.str();
    return 0;
  }
  virtual int handle_input(ndk::ndk_handle )
  {
    int result = this->peer().recv(this->recv_buff_->wr_ptr(),
                                   this->recv_buff_->size() -
                                   this->recv_buff_->length());
    if (result < 0)
    {
      if (errno == EWOULDBLOCK)
        return 0;
      net_log->error("[%s] connection terminate ! [%s]"
                     , strerror(errno)
                     , this->remote_addr_.c_str());
      return -1;
    }else if (result == 0)
    {
      assert((this->recv_buff_->size() - this->recv_buff_->length()) > 0);
      net_log->debug("connection closed by peer [%s]! [escape %d secs]"
                     , this->remote_addr_.c_str()
                     , (int)difftime(::time(0), this->begin_time_));
      return -1;
    }
    this->recv_buff_->wr_ptr(result);
    if (!this->recv_msg_ok_)
    {
      char *ptr = this->recv_buff_->data();
      char *p = ::strstr(ptr, "\r\n\r\n");
      if (p == 0)
      {
        net_log->rinfo("[%s] recv an uncompletd request [%s]"
                       , this->remote_addr_.c_str(),
                       ptr);
        return 0;
      }
    }
    return this->handle_data(this->recv_buff_);
  }
  virtual int handle_timeout(const void *, const ndk::time_value &)
  {
    if (this->reconnect_num_ >= 2)
    {
      net_log->rinfo("reconnect [%s] too many", this->remote_addr_.c_str());
      return -1;
    }
    net_log->rinfo("reconnect [%s] [%p]", 
                   this->remote_addr_.c_str(),
                   this);
    this->peer().close();
    ndk::time_value tout(3, 0);
    ndk::inet_addr remote_addr(this->remote_addr_.c_str());
    g_connector->connect(this, remote_addr, &tout);
    ++this->reconnect_num_;
    return 0;
  }
  virtual int handle_data(ndk::message_block *mb) = 0;
protected:
  int connect_ok_;
  int recv_msg_ok_;
  int reconnect_num_;
  time_t begin_time_;
  std::string fileid_;
  std::string remote_addr_;
  ndk::message_block *recv_buff_;
};
// 
class scs_interface : public interface
{
public:
  scs_interface(const std::string &fileid,
                const std::string &addr)
    : interface(fileid, addr),
    recv_bytes_(0),
    content_length_(0),
    recived_stream_time_(0)
  { 
    ++g_scs_req_total;
  }

  ~scs_interface()
  {
  }

  virtual int handle_data(ndk::message_block *mb)
  {
    if (this->recv_msg_ok_)
    {
      if (svc_status == svc_term)
        return -1;
      if (this->recived_stream_time_ == 0)
      {
        this->recived_stream_time_ = ::time(0);
        if (difftime(this->recived_stream_time_,
                     this->begin_time_) > 2)
          ++g_scs_stream_timeout;
        net_log->rinfo("load [%s] stream successfully! [used %d secs]"
                       , this->fileid_.c_str()
                       , (int)difftime(this->recived_stream_time_, 
                                       this->begin_time_));
      }
      g_scs_flux_total  += this->recv_buff_->length();
      g_period_flux     += this->recv_buff_->length();
      this->recv_bytes_ += this->recv_buff_->length();
      this->recv_buff_->reset();
      //
      if (this->recv_bytes_ >= this->content_length_)
        return -1;
      return 0;
    }
    if (difftime(::time(0), this->begin_time_) > 1)
    {
      net_log->error("scs [%s] response timeout! escape %d secs",
                     this->remote_addr_.c_str(),
                     (int)difftime(::time(0), this->begin_time_));
      ++g_scs_req_timeout;
    }
    char *ptr = mb->data();
    ptr += sizeof("HTTP/1.1");
    if (ptr[0] != '2' || ptr[1] != '0' || ptr[2] != '0')
    {
      ++g_scs_req_failed_404;
      net_log->error("scs [%s] response status isn't 200! [is %c%c%c]"
                     , this->remote_addr_.c_str()
                     , ptr[0]
                     , ptr[1]
                     , ptr[2]);
      return -1;
    }
    char *p = ::strstr(ptr, "\r\n\r\n");
    int head_length = p + 4 - ptr;
    int content_length = 0;
    p = ::strstr(ptr, "Content-Length: ");
    if (p != 0)
    {
      this->content_length_ = ::atoi(p + sizeof("Content-Length: ") - 1);
      this->recv_bytes_ += mb->length() - head_length;
      g_scs_flux_total  += mb->length() - head_length; 
      g_period_flux     += mb->length() - head_length;
      if (this->recv_bytes_ >= this->content_length_)
      {
        this->recv_msg_ok_ = 1;
        current_payload++;
        return -1;
      }
      this->recv_buff_->reset();
    }else
    {
      net_log->error("not found 'Content-Length' in scs's response!");
      return -1;
    }
    this->recv_msg_ok_ = 1;
    current_payload++;
    return 0;
  }
  virtual int handle_close(ndk::ndk_handle , ndk::reactor_mask m)
  {
    if (!this->connect_ok_)
    {
      ++g_scs_con_failed;
      net_log->rinfo("connect scs interface [%s] failed! mask = %d [%s]",
                     this->remote_addr_.c_str(),
                     m,
                     strerror(errno));
    }else
    {
      if (this->recv_msg_ok_ == 0)
      {
        net_log->rinfo("request [%s] failed![%s]", 
                       this->fileid_.c_str(),
                       this->recv_buff_->data());
      }else
      {
        current_payload--;
        if (this->recv_bytes_ >= this->content_length_)
        {
          ++g_scs_req_completed;
          net_log->rinfo("download [%s] completely! [flux = %d bytes, "
                         "used %d secs]"
                         , this->fileid_.c_str()
                         , this->recv_bytes_
                         , (int)difftime(::time(0), this->begin_time_));
        }else
        {
          net_log->rinfo("download [%s] not completely! [flux = %d bytes, "
                         "used %d secs]"
                         , this->fileid_.c_str()
                         , this->recv_bytes_
                         , (int)difftime(::time(0), this->begin_time_));
        }
      }
    }
    this->destroy();
    return 0;
  }
protected:
  int recv_bytes_;
  int content_length_;
  int recived_stream_time_;
  std::deque<ndk::message_block *> data_blocks_;
};
class sbs_interface : public interface
{
public:
  sbs_interface(const std::string &fileid,
                const std::string &addr)
    : interface(fileid, addr)
  { 
    ++g_sbs_req_total;
  }
  virtual int handle_data(ndk::message_block *mb)
  {
    this->recv_msg_ok_ = 1;
    if (difftime(::time(0), this->begin_time_) > 2)
    { 
      ++g_sbs_req_timeout;
      return -1;
    }
    char *ptr = mb->data();
    ptr += sizeof("HTTP/1.1");
    if (ptr[0] != '3' || ptr[1] != '0' || ptr[2] != '2')
    {
      net_log->error("sbs [%s] response status isn't 302! [is %c%c%c]"
                     , this->remote_addr_.c_str()
                     , ptr[0]
                     , ptr[1]
                     , ptr[2]);
      ++g_sbs_req_failed;
      return -1;
    }
    char *p = ::strstr(ptr, "Location: ");
    if (p == 0)
    {
      net_log->error("sbs [%s] not found 'Locaction: '"
                     , this->remote_addr_.c_str());
    }else
    {
      char *pp = ::strstr(p, "\r\n");
      if (pp == 0)
        net_log->error("sbs [%s] not found `Locaction: `'s '\\r\\n'"
                       , this->remote_addr_.c_str());
      else
      {
        ++g_sbs_req_succ;
        std::string location;
        location.assign(p + sizeof("Location:"), 
                        pp - (p + sizeof("Location:")));
        std::string host = help::parse_hostname(location);
        if (host.find(':') == std::string::npos)
          host.append(":80");
        ndk::inet_addr remote_addr(host.c_str());
        scs_interface *scs = new scs_interface(location,
                                               host);
        if (multi_thread)
        {
          g_assign_task_task->putq(new ndk::message_block((char *)new assign_task_task::job_item(scs,
                                                                                                 remote_addr), 
                                                          sizeof(void*)));
        }else
        {
          ndk::time_value tout(3, 0);
          g_connector->connect(scs, remote_addr, &tout);
        }
        return -1;
      }
    }
    ++g_sbs_req_failed;
    return -1;
  }
  virtual int handle_close(ndk::ndk_handle , ndk::reactor_mask )
  {
    if (!this->connect_ok_)
    {
      ++g_sbs_con_failed;
      net_log->rinfo("connect sbs interface [%s] failed!",
                     this->remote_addr_.c_str());
    }
    if (this->recv_msg_ok_ == 0)
      ++g_sbs_req_failed;
    this->destroy();
    return 0;
  }
};
class biz_interface : public interface
{
public:
  biz_interface(const std::string &filied,
                const std::string &addr)
    : interface(filied, addr)
  { 
    ++g_biz_req_total;
  }

  virtual int handle_data(ndk::message_block *mb)
  {
    this->recv_msg_ok_ = 1;
    if (difftime(::time(0), this->begin_time_) > 2)
    { 
      ++g_biz_req_timeout;
      return -1;
    }
    char *ptr = mb->data();
    ptr += sizeof("HTTP/1.1");
    if (ptr[0] != '3' || ptr[1] != '0' || ptr[2] != '2')
    {
      net_log->error("biz [%s] response status isn't 302! [is %c%c%c]"
                     , this->remote_addr_.c_str()
                     , ptr[0]
                     , ptr[1]
                     , ptr[2]);
      ++g_biz_req_failed;
      return -1;
    }
    char *p = ::strstr(ptr, "Location: ");
    if (p == 0)
    {
      net_log->error("biz [%s] not found 'Locaction: '"
                     , this->remote_addr_.c_str());
    }else
    {
      char *pp = ::strstr(p, "\r\n");
      if (pp == 0)
        net_log->error("biz [%s] not found `Locaction: `'s '\\r\\n'"
                       , this->remote_addr_.c_str());
      else
      {
        ++g_biz_req_succ;
        std::string location;
        location.assign(p + sizeof("Location:"), 
                        pp - (p + sizeof("Location:")));
        std::string host = help::parse_hostname(location);
        if (host.find(':') == std::string::npos)
          host.append(":80");
        ndk::inet_addr remote_addr(host.c_str());
        sbs_interface *sbs = new sbs_interface(location,
                                               host);
        if (multi_thread)
        {
          g_assign_task_task->putq(new ndk::message_block((char *)new assign_task_task::job_item(sbs,
                                                                                                 remote_addr), 
                                                          sizeof(void*)));
        }else
        {
          ndk::time_value tout(3, 0);
          g_connector->connect(sbs, remote_addr, &tout);
        }
        return -1;
      }
    }
    ++g_biz_req_failed;
    return -1;
  }
  virtual int handle_close(ndk::ndk_handle , ndk::reactor_mask m)
  {
    if (!this->connect_ok_)
    {
      ++g_biz_con_failed;
      net_log->rinfo("connect biz interface [%s] failed!",
                     this->remote_addr_.c_str());
    }
    if (this->recv_msg_ok_ == 0)
      ++g_biz_req_failed;
    this->destroy();
    return 0;
  }
};
class calc_bandwidth_timer : public ndk::event_handler
{
public:
  calc_bandwidth_timer()
  {
    this->last_calc_time_.update();
  }
  virtual int handle_timeout(const void * ,
                             const ndk::time_value &now)
  {
    ndk::time_value diff = now - this->last_calc_time_;
    long long int inter = (long long int)diff.msec();
    g_scs_bandwidth = g_period_flux*8*1000/inter/1024;
    g_period_flux = 0;
    this->last_calc_time_ = now;
    return 0;
  }
protected:
  ndk::time_value last_calc_time_;
};
class assign_task_timer : public ndk::event_handler
{
public:
  assign_task_timer()
    : timer_id_(-1)
  { }
  int timer_id()
  {
    return this->timer_id_;
  }
  void timer_id(int val)
  {
    this->timer_id_ = val;
  }
  int load_urls(const char *name)
  {
    if (name == 0)
      name = this->file_name_.c_str();
    else
      this->file_name_ = name;
    std::ifstream ifs(name, std::ios::binary | std::ios::in);
    if (!ifs)
      return -1;
    char line[4096] = {0};
    this->url_list_.clear();
    this->unassinged_url_list_.clear();
    while (ifs.getline(line, 4095))
    {
      char *p = ::strchr(line, '\r');
      if (p) *p = '\0';
      this->url_list_.push_back(line);
    }
    ifs.close();
    return this->url_list_.empty() ? -1 : 0;
  }
  virtual int handle_timeout(const void * ,
                             const ndk::time_value & )
  {
    if (svc_status != svc_running)
    {
      if (svc_status == svc_stop) return 0;
      if (svc_status == svc_exit) 
      {
        ndk::reactor::instance()->close();
        ::exit(0);
      }
      return 0;
    }
    if (this->unassinged_url_list_.empty())
    {
      this->unassinged_url_list_.assign(this->url_list_.begin(),
                                        this->url_list_.end());
      std::random_shuffle(this->unassinged_url_list_.begin(),
                          this->unassinged_url_list_.end());
    }
    int count = 0;
    std::deque<std::string>::iterator iter;
    for (iter = this->unassinged_url_list_.begin();
         iter != this->unassinged_url_list_.end()
         && count < concurrent_number;
         ++count)
    {
      if (current_payload >= concurrent_number_keeping)
        return 0;
      std::string host = help::parse_hostname(*iter);
      if (host.find(':') == std::string::npos)
        host.append(":80");
      interface *si = 0;
      switch (start_interface)
      {
      case I_BIZ:
        si = new biz_interface(*iter, host); break;
      case I_SBS:
        si = new sbs_interface(*iter, host); break;
      case I_SCS:
        si = new scs_interface(*iter, host); break;
      default:
        net_log->error("error start interface type: %d", start_interface);
        break;
      }
      iter = this->unassinged_url_list_.erase(iter);
      ndk::inet_addr remote_addr(host.c_str());
      if (si)
      {
        if (multi_thread)
        {
          g_assign_task_task->putq(new ndk::message_block((char *)new assign_task_task::job_item(si,
                                                                                                 remote_addr), 
                                                          sizeof(void*)));
        }else
        {
          ndk::time_value tout(3, 0);
          g_connector->connect(si, remote_addr, &tout);
        }
      }
    }
    return 0;
  }
protected:
  int timer_id_;
  std::string file_name_;
  std::deque<std::string> url_list_;
  std::deque<std::string> unassinged_url_list_;
};
//
class cmd_interface;
ndk::acceptor<cmd_interface> *g_acceptor = 0;
class cmd_interface : public ndk::svc_handler
{
public:
  cmd_interface()
  {
    this->recv_buff_ = 0;
  }
  virtual ~cmd_interface()
  {
    if (this->recv_buff_)
    {
      this->recv_buff_->release();
    }
    this->recv_buff_ = 0;
  }
  virtual int open(void *arg)
  {
    this->recv_buff_ = new ndk::message_block(4096);
    if (this->get_reactor() 
        && this->get_reactor()->register_handler
        (this, 
         event_handler::read_mask) == -1)
      return -1;
    return 0;
  }
  virtual int handle_input(ndk::ndk_handle )
  {
    int result = this->peer().recv(this->recv_buff_->wr_ptr(),
                                   this->recv_buff_->size() -
                                   this->recv_buff_->length());
    if (result < 0)
    {
      if (errno == EWOULDBLOCK)
        return 0;
      return -1;
    }else if (result == 0)
      return -1;
    this->recv_buff_->wr_ptr(result);

    return this->handle_data(this->recv_buff_);
  }
  virtual int handle_close(ndk::ndk_handle , ndk::reactor_mask m)
  {
    STRACE("");
    assert(m == event_handler::read_mask);
    this->destroy();
    return 0;
  }
  int handle_data(ndk::message_block *mb)
  {
    char *ptr = mb->data();
    ptr[mb->length()] = '\0';
    char *p = ::strstr(ptr, "\r\n\r\n");
    int head_length = p + 4 - ptr;
    if (p == 0)
    {
      net_log->rinfo("recv an uncompletd request");
      return 0;
    }
    if (ptr[0] != 'G' || ptr[1] != 'E' || ptr[2] != 'T')
    {
      net_log->rinfo("recv an unknown request [%s]", ptr);
      return -1;
    }
    p = ::strchr(ptr + 3 + 1 + 1, ' ');
    if (p == 0) return -1;
    std::string cmd;
    cmd.assign(ptr + 3 + 1 + 1, p - (ptr + 3 + 1 + 1));
    return this->handle_cmd(cmd);
  }
  int handle_cmd(const std::string &cmd)
  {
    std::string resp = "ok\n";
    if (cmd  == "start")
    {
      svc_status = svc_running;
    }else if (cmd == "stop")
    {
      svc_status = svc_stop;
    }else if (cmd == "term")
    {
      svc_status = svc_term;
    }else if (cmd == "reload")
    {
      g_assign_task->load_urls(0);
      clear_statistic_data();
    }else if (cmd == "clear")
    {
      clear_statistic_data();
    }else if (cmd == "exit")
    {
      svc_status = svc_exit;
    }else if (!cmd.empty() && cmd[0] == '?')
    {
      const char *p = cmd.c_str();
      std::string urls_filename;
      int new_start_interface = start_interface;
      int reload_urls = 0;
      do
      {
        p += 1;
        switch (*p)
        {
        case 'c':
          concurrent_number = ::atoi(p + 2);
          break;
        case 'k':
          concurrent_number_keeping = ::atoi(p + 2);
          break;
        case 'I':
          {
            interval_period = ::atoi(p + 2);
            ndk::time_value tv(interval_period, 0);
            if (ndk::reactor::instance()->reset_timer_interval(g_assign_task->timer_id(),
                                                               tv) != 0)
              net_log->error("reset timer interval failed!");
            else
              net_log->rinfo("reset timer interval to [%d] ok!", interval_period);
          }
          break;
        case 'i':
          {
            char *pp = ::strchr((char *)(p + 2), '&');
            if (pp == 0)
              urls_filename.assign(p + 2);
            else
              urls_filename.assign(p + 2, pp - p - 2);
            reload_urls = 1;
          }
          break;
        case 'p':
          {
            int port = ::atoi(p + 2);
            if (port != listen_port)
            {
              g_acceptor->close();
              listen_port = port;
              ndk::inet_addr local_addr(listen_port);
              if (g_acceptor->open(local_addr, ndk::reactor::instance()) != 0)
              {
                net_log->error("listen port [%d] failed!");
              }
            }
          }
          break;
        case 't':
          if (::strncmp(p + 2, "biz", 3) == 0)
            new_start_interface = I_BIZ;
          else if (::strncmp(p + 2, "sbs", 3) == 0)
            new_start_interface = I_SBS;
          else if (::strncmp(p + 2, "scs", 3) == 0)
            new_start_interface = I_SCS;
          else
          {
            resp = this->dump_help();
            break;
          }
          break;
        default:
          resp = this->dump_help();
          break;
        } // end of switch
        p = ::strchr(p, '&');
      }while(p != 0);
      if (new_start_interface != start_interface)
      {
        if (reload_urls == 0)
          resp = "this parameter of `t' is changed, but `i' isn't reset\n";
        else
          start_interface = new_start_interface;
      }
      if (reload_urls)
      {
        if (urls_filename.empty())
          g_assign_task->load_urls(0);
        else
          g_assign_task->load_urls(urls_filename.c_str());
      }
    }else if (cmd == "status")
    {
      static int v1 = 10, v2 = 10, v3 = 10, v4 = 10, v5 = 10, v6 = 10, v7 = 10;
      static int v8 = 12, v9 = 12;
      static char biz_req_total[]     = "total";
      static int  biz_req_total_w     = v1;
      static char biz_req_succ[]      = "success";
      static int  biz_req_succ_w      = v2;
      static char biz_req_failed[]    = "failed";
      static int  biz_req_failed_w    = v3;
      static char biz_con_failed[]    = "err-con";
      static int  biz_con_failed_w    = v4;
      static char biz_req_timeout[]   = "timeout";
      static int  biz_req_timeout_w   = v5;

      static char sbs_req_total[]     = "total";
      static int  sbs_req_total_w     = v1;
      static char sbs_req_succ[]      = "success";
      static int  sbs_req_succ_w      = v2;
      static char sbs_con_failed[]    = "err-con";
      static int  sbs_con_failed_w    = v3;
      static char sbs_req_failed[]    = "failed";
      static int  sbs_req_failed_w    = v4;
      static char sbs_req_timeout[]   = "timeout";
      static int  sbs_req_timeout_w   = v5;

      static char scs_req_total[]     = "total";
      static int  scs_req_total_w     = v1;
      static char scs_req_completed[] = "completed";
      static int  scs_req_completed_w = v2;
      static char scs_con_failed[]    = "err-con";
      static int  scs_con_failed_w    = v3;
      static char scs_req_failed_404[] = "err-404";     
      static int  scs_req_failed_404_w = v4;
      static char scs_req_timeout[]   = "req-tout";
      static int  scs_req_timeout_w   = v5;
      static char scs_stream_timeout[]   = "stm-tout";
      static int  scs_stream_timeout_w   = v6;
      static char scs_cur_payload[]   = "payload";
      static int  scs_cur_payload_w   = v7;

      static char scs_flux_total[]    = "total(mB)";
      static int  scs_flux_total_w    = v8;
      static char scs_bandwidth[]     = "bwidth(kB)";
      static int  scs_bandwidth_w     = v9;

      std::ostringstream  ostr;
      ostr << std::setfill(' ')
        << std::setiosflags(std::ios::left);

      if (start_interface == I_BIZ)
      {
        ostr << std::setw(sizeof("biz:")) << "BIZ:"
          << std::setw(biz_req_total_w) << biz_req_total
          << std::setw(biz_req_succ_w) << biz_req_succ
          << std::setw(biz_con_failed_w) << biz_con_failed
          << std::setw(biz_req_failed_w) << biz_req_failed
          << std::setw(biz_req_timeout_w) << biz_req_timeout;
        ostr << std::endl;
        ostr << std::setw(sizeof("biz:")) << " "
          << std::setw(biz_req_total_w) << g_biz_req_total
          << std::setw(biz_req_succ_w) << g_biz_req_succ
          << std::setw(biz_con_failed_w) << g_biz_con_failed
          << std::setw(biz_req_failed_w) << g_biz_req_failed
          << std::setw(biz_req_timeout_w) << g_biz_req_timeout;
        ostr << std::endl;
      }

      if (start_interface == I_BIZ || start_interface == I_SBS)
      {
        ostr << std::setw(sizeof("sbs:")) << "SBS:"
          << std::setw(sbs_req_total_w) << sbs_req_total
          << std::setw(sbs_req_succ_w) << sbs_req_succ
          << std::setw(sbs_con_failed_w) << sbs_con_failed
          << std::setw(sbs_req_failed_w) << sbs_req_failed
          << std::setw(sbs_req_timeout_w) << sbs_req_timeout;
        ostr << std::endl;
        ostr << std::setw(sizeof("sbs:")) << " "
          << std::setw(sbs_req_total_w) << g_sbs_req_total
          << std::setw(sbs_req_succ_w) << g_sbs_req_succ
          << std::setw(sbs_con_failed_w) << g_sbs_con_failed
          << std::setw(sbs_req_failed_w) << g_sbs_req_failed
          << std::setw(sbs_req_timeout_w) << g_sbs_req_timeout;
        ostr << std::endl;
      }

      ostr << std::setw(sizeof("scs:")) << "SCS:"
        << std::setw(scs_req_total_w) << scs_req_total
        << std::setw(scs_req_completed_w) << scs_req_completed
        << std::setw(scs_con_failed_w) << scs_con_failed
        << std::setw(scs_req_failed_404_w) << scs_req_failed_404
        << std::setw(scs_req_timeout_w) << scs_req_timeout
        << std::setw(scs_stream_timeout_w) << scs_stream_timeout
        << std::setw(scs_cur_payload_w) << scs_cur_payload 
        //<< std::setw(sizeof("|")) << "|"
        << std::setw(sizeof("flux:")) << "FLUX:"
        << std::setw(scs_flux_total_w) << scs_flux_total
        << std::setw(scs_bandwidth_w) << scs_bandwidth
        << std::endl;

      ostr  << std::setw(sizeof("scs:")) << " "
        << std::setw(scs_req_total_w) << g_scs_req_total
        << std::setw(scs_req_completed_w) << g_scs_req_completed
        << std::setw(scs_con_failed_w) << g_scs_con_failed
        << std::setw(scs_req_failed_404_w) << g_scs_req_failed_404
        << std::setw(scs_req_timeout_w) << g_scs_req_timeout
        << std::setw(scs_stream_timeout_w) << g_scs_stream_timeout
        << std::setw(scs_cur_payload_w) << current_payload

        //<< std::setw(sizeof("|")) << " "

        << std::setw(sizeof("flux:")) << " "
        << std::setw(scs_flux_total_w) << g_scs_flux_total/1024/1024
        << std::setw(scs_bandwidth_w) << g_scs_bandwidth;

      ostr << std::endl;
      resp = ostr.str();
    }else
    {
      resp = this->dump_help();
    }

    std::ostringstream os;
    os << "HTTP/1.1 200 OK\r\n"
      << "Server: vodperf\r\n"
      << "Accept-Ranges: bytes\r\n"
      << "Connection: close\r\n"
      << "Content-Length: " << resp.length() << "\r\n"
      << "\r\n" << resp;
    this->peer().send(os.str().c_str(), os.str().length());
    return -1;
  }
  std::string dump_help()
  {
    std::ostringstream  ostr;
    ostr << std::setfill(' ')
      << std::setiosflags(std::ios::right);
    ostr << std::setw(12) << "command" << " / " << std::endl
      << std::setw(12) << "start" << " : " << "start run service" << std::endl
      << std::setw(12) << "stop" << " : " << "stop to launch new tasks" << std::endl
      << std::setw(12) << "term" << " : " << "terminate all tasks(including luanched tasks)" << std::endl
      << std::setw(12) << "reload" << " : " << "reload task file" << std::endl
      << std::setw(12) << "clear" << " : " << "clear statistic data" << std::endl
      << std::setw(12) << "exit" << " : " << "exit program" << std::endl
      << std::setw(12) << "status" << " : " << "show statistic data" << std::endl
      << std::setw(12) << "e.g." << " : " << "http://host/status" << std::endl
      << std::endl
      << std::setw(12) << "setting" << " /? " << std::endl
      << std::setw(12) << "c" << " : " << "number of multiple requests to perform at a time" << std::endl
      << std::setw(12) << "k" << " : " << "number of requests keeping" << std::endl
      << std::setw(12) << "I" << " : " << "interval of every group request" << std::endl
      << std::setw(12) << "i" << " : " << "name of file where urls saved" << std::endl
      << std::setw(12) << "t" << " : " << "starting interface, 'biz' or 'sbs' or 'scs'" << std::endl
      << std::setw(12) << "p" << " : " << "listen port" << std::endl
      << std::setw(12) << "e.g." << " : " << "http://host/?c=10&k=200i=url" << std::endl;
    return ostr.str();
  }
protected:
  int timer_id_;
  ndk::message_block *recv_buff_;
};
void print_usage()
{
  printf("Usage: vodperf [OPTION...]\n\n");
  printf("  -c  number            Number of multiple requests to perform at a time\n");
  printf("  -k  number            Number of requests keeping\n"); 
  printf("  -I  secs              Interval of every group request\n");
  printf("  -i  filename          Name of file where urls saved\n");
  printf("  -t  type              'biz' or 'sbs' or 'scs'\n");
  printf("  -P  poll type         's'(select) or 'e'(epoll)\n");
  printf("  -p  number            Listen port(default is 8800)\n");
  printf("  -m                    Use multi thread\n");
}
int main(int argc, char *argv[])
{
  if (argc == 1)
  {
    print_usage();
    return 0;
  }
  int c = -1;
  const char *opt = "c:k:I:i:t:p:P:m";
  extern int optind, optopt;
  std::string urls_filename;
  std::string poll_type = "e";
  while ((c = getopt(argc, argv, opt)) != -1)
  {
    switch(c)
    {
    case 'c':
      concurrent_number = ::atoi(optarg);
      break;
    case 'k':
      concurrent_number_keeping = ::atoi(optarg);
      break;
    case 'I':
      interval_period = ::atoi(optarg);
      break;
    case 'i':
      urls_filename = optarg;
      break;
    case 't':
      if (::strcmp(optarg, "biz") == 0)
        start_interface = I_BIZ;
      else if (::strcmp(optarg, "sbs") == 0)
        start_interface = I_SBS;
      else if (::strcmp(optarg, "scs") == 0)
        start_interface = I_SCS;
      else
        return -1;
      break;
    case 'p':
      listen_port = ::atoi(optarg);
      break;
    case 'P':
      poll_type = optarg;
      break;
    case 'm':
      multi_thread = 1;
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
  ndk::reactor_impl *r_impl = 0;
  if (poll_type == "s")
  {
    if (multi_thread)
    {
      ndk::select_reactor<ndk::reactor_token> *r =
        new ndk::select_reactor<ndk::reactor_token>();
      if (r->open() != 0)
      {
        fprintf(stderr, "open select reactor failed\n");
        return -1;
      }
      r_impl = r;
    }else
    {
      ndk::select_reactor<ndk::reactor_null_token> *r =
        new ndk::select_reactor<ndk::reactor_null_token>();
      if (r->open() != 0)
      {
        fprintf(stderr, "open select reactor failed\n");
        return -1;
      }
      r_impl = r;
    }
  }else
  {
    if (multi_thread)
    {
      ndk::epoll_reactor<ndk::reactor_token> *r = 
        new ndk::epoll_reactor<ndk::reactor_token>();
      if (r->open() != 0)
      {
        fprintf(stderr, "open epoll reactor failed\n");
        return -1;
      }
      r_impl = r;
    }else
    {
      ndk::epoll_reactor<ndk::reactor_null_token> *r = 
        new ndk::epoll_reactor<ndk::reactor_null_token>();
      if (r->open() != 0)
      {
        fprintf(stderr, "open epoll reactor failed\n");
        return -1;
      }
      r_impl = r;
    }
  }
  ndk::reactor::instance(new ndk::reactor(r_impl));

  g_acceptor = new ndk::acceptor<cmd_interface>();
  ndk::inet_addr local_addr(listen_port);
  if (g_acceptor->open(local_addr, ndk::reactor::instance()) != 0)
  {
    fprintf(stderr, "open acceptor failed\n");
    return -1;
  }

  g_connector = new ndk::connector<interface>();
  g_connector->open(ndk::reactor::instance());

  if (multi_thread)
  {
    g_assign_task_task = new assign_task_task();
    if (g_assign_task_task->open() != 0)
    {
      fprintf(stderr, "assign task task open failed!\n");
      return 0;
    }
  }

  g_assign_task = new assign_task_timer();
  if (g_assign_task->load_urls(urls_filename.c_str()) != 0)
  {
    fprintf(stderr, "load urls failed\n");
    return 0;
  }
  int timer_id = ndk::reactor::instance()->schedule_timer(g_assign_task,
                                                          0,
                                                          ndk::time_value(5, 0),
                                                          ndk::time_value(interval_period, 0));
  if (timer_id  == -1)
  {
    fprintf(stderr, "schedule assign task timer failed!\n");
    return 0;
  }
  g_assign_task->timer_id(timer_id);
  timer_id = ndk::reactor::instance()->schedule_timer(new calc_bandwidth_timer(),
                                                          0,
                                                          ndk::time_value(2, 0),
                                                          ndk::time_value(2, 0));
  if (timer_id  == -1)
  {
    fprintf(stderr, "schedule calc bandwidth timer failed!\n");
    return 0;
  }

  ndk::reactor::instance()->run_reactor_event_loop();
  net_log->error("reactor exit");
  return 0;
}

