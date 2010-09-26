// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-21 21:57
 * Brief    : For testing <epoll_reactor>, <acceptor>.
 */
//========================================================================

#include <cassert>

#include <ndk/reactor.h>
#include <ndk/svc_handler.h>
#include <ndk/message_block.h>
#include <ndk/acceptor.h>
#include <ndk/task.h>
#include <ndk/logger.h>
#include <ndk/epoll_reactor.h>
#include <ndk/select_reactor.h>

//#define m_thread 1
#define epoll_r 0
static ndk::logger *net_log = ndk::log_manager::instance()->get_logger("root.httpd");

class reactor_task : public ndk::task
{
public:
  reactor_task()
    : ndk::task()
  { }

  virtual int open(void *arg)
  {
    this->set_reactor(reinterpret_cast<ndk::reactor *>(arg));
    return this->activate();
  }
  virtual int svc()
  {
    this->get_reactor()->run_reactor_event_loop();
  }
};
typedef ndk::singleton<reactor_task> s_reactor_task;

class recv_data : public ndk::svc_handler
{
public:
  recv_data()
  {
    this->recv_buff_ = 0;
    this->timer_id_ = -1;
  }
  virtual ~recv_data()
  {
    if (this->recv_buff_)
    {
      delete []this->recv_buff_->data();
      this->recv_buff_->release();
    }
    this->recv_buff_ = 0;
  }
  virtual int open(void *arg)
  {
    net_log->debug("new connection");
    char *p = new char[4096];
    this->recv_buff_ = new ndk::message_block(p, 4096);
#ifdef m_thread
    this->set_reactor(s_reactor_task::instance()->get_reactor());
#endif
    if (this->get_reactor() 
        && this->get_reactor()->register_handler
        (this, 
         event_handler::read_mask) == -1)
      return -1;
    this->begin_time_ = ndk::time_value::gettimeofday();
    ndk::time_value tv(20, 0);
    this->timer_id_ = this->get_reactor()->schedule_timer(this,
                                                          0,
                                                          tv);
    net_log->debug("timer id = %d", this->timer_id_);
    assert(this->timer_id_ != -1);
    return this->timer_id_ == -1 ? -1 : 0;
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
  virtual int handle_timeout(const void *, const ndk::time_value &now)
  {
    ndk::time_value tv = now - this->begin_time_;
    net_log->error("recv data timeout [escape %d.%d]",
                   tv.sec(),
                   (int)tv.msec());
    return 0;
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
    if (ptr[0] == 'G' || ptr[1] == 'E' || ptr[2] == 'T')
    {
      net_log->rinfo("recv a [GET] request");
    }else if (ptr[0] == 'P' || ptr[1] == 'O' 
              || ptr[2] == 'S' || ptr[3] == 'T')
    {
      net_log->rinfo("recv a [POST] request");
    }else if (ptr[0] == 'H' || ptr[1] == 'E' 
              || ptr[2] == 'A' || ptr[3] == 'D')
    {
      net_log->rinfo("recv a [HEAD] request");
    }else
    {
      net_log->rinfo("recv an unknown request [%s]", ptr);
    }

    int content_length = 0;
    p = ::strstr(ptr, "Content-Length:");
    if (p)
    {
      content_length = ::atoi(p + sizeof("Content-Length:") - 1);
      if ((content_length + head_length) >= mb->size())
      {
        net_log->rinfo("the request [%s] is too long", ptr);
        return -1;
      }
      if (mb->length() < (content_length + head_length))
        return 0;
    }
    char resp[] = 
      "HTTP/1.1 200 OK\r\n"
      "Server: httpd\r\n"
      "Accept-Ranges: bytes\r\n"
      "Connection: close\r\n\r\n";
    this->peer().send(resp, sizeof(resp) - 1);
    return -1;
  }
protected:
  int timer_id_;
  ndk::time_value begin_time_;
  ndk::message_block *recv_buff_;
};

ndk::acceptor<recv_data> *g_acceptor = 0;
static ndk::logger *main_log = ndk::log_manager::instance()->get_logger("root.httpd");

int main(int argc, char *argv[])
{
  if (ndk::log_manager::instance()->init("logger-cfg.ini") != 0)
  {
    fprintf(stderr, "init logger failed\n");
    return 0;
  }
#ifdef epoll_r 
  ndk::epoll_reactor<ndk::reactor_null_token> *r_impl
    = new ndk::epoll_reactor<ndk::reactor_null_token>();
  if (r_impl->open() != 0)
  {
    main_log->error("open epoll reactor failed");
    return -1;
  }
#else
  ndk::select_reactor<ndk::reactor_null_token> *r_impl
    = new ndk::select_reactor<ndk::reactor_null_token>();
  if (r_impl->open() != 0)
  {
    main_log->error("open select reactor failed");
    return -1;
  }
# ifdef m_thread
  ndk::epoll_reactor<ndk::reactor_token> *er
    = new ndk::epoll_reactor<ndk::reactor_token>();
  if (er->open() != 0)
  {
    main_log->error("open epoll reactor failed");
    return -1;
  }
  s_reactor_task::instance()->open(new ndk::reactor(er));
# endif
#endif
  ndk::reactor::instance(new ndk::reactor(r_impl));
  g_acceptor = new ndk::acceptor<recv_data>();
  ndk::inet_addr local_addr(8880);
  if (g_acceptor->open(local_addr, ndk::reactor::instance()) != 0)
  {
    main_log->error("open acceptor failed");
    return -1;
  }
  ndk::reactor::instance()->run_reactor_event_loop();
  main_log->error("reactor exit");
  return 0;
}

