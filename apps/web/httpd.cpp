// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-21 21:57
 * Brief    : For testing <epoll_reactor>, <acceptor>.
 */
//========================================================================

#include <ndk/svc_handler.h>
#include <ndk/message_block.h>
#include <ndk/reactor.h>
#include <ndk/acceptor.h>
#include <ndk/ndk_log.h>
#include <ndk/epoll_reactor.h>
#include <ndk/multi_reactor.h>
#include <ndk/mem_pool.h>
#include <ndk/ndk_os.h>

static ndk_log *net_log = log_manager::instance()->get_logger("net");
class recv_data : public ndk::svc_handler
{
public:
  recv_data()
  {
    this->recv_buff_ = new message_block(4096);
  }
  virtual ~recv_data()
  {
    if (this->recv_buff_)
      this->recv_buff_->release();
    this->recv_buff_ = 0;
  }
  virtual int open(void *arg)
  {
    //net_log->puts("new connection\n", sizeof ("new connection"));
    net_log->debug("new connection");
    if (this->get_reactor() 
        && this->get_reactor()->register_handler
        (this, 
         event_handler::read_mask) == -1)
      return -1;
    return 0;
  }
  virtual int handle_input(ndk_handle )
  {
    int result = this->peer().recv(this->recv_buff_->wr_ptr(),
                                   this->recv_buff_->size() -
                                   this->recv_buff_->length());
    if (result < 0)
    {
      if (ndk_os::last_error() == EWOULDBLOCK)
        return 0;
      return -1;
    }else if (result == 0)
      return -1;
    this->recv_buff_->wr_ptr(result);

    return this->handle_data(this->recv_buff_);
  }
  virtual int handle_close(ndk_handle , reactor_mask )
  {
    STRACE("");
    this->destroy();
    return 0;
  }
  int handle_data(message_block *mb)
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
  message_block *recv_buff_;
};

//#define use_multi_reactor

ndk::acceptor<recv_data> *g_acceptor = 0;
static ndk::ndk_log *main_log = ndk::log_manager::instance()->get_logger("main");
int main(int argc, char *argv[])
{
  if (ndk::log_manager::instance()->init("../logger_config.ini") != 0)
  {
    fprintf(stderr, "init logger failed\n");
    return 0;
  }
#ifndef use_multi_reactor
  ndk::epoll_reactor *ep = new ndk::epoll_reactor();
  if (ep->open() != 0)
  {
    main_log->error("open epoll reactor failed");
    return -1;
  }
  ndk::reactor::instance(new ndk::reactor(ep));
#else
  ndk::multi_reactor<ndk::epoll_reactor> *mp = 
    new ndk::multi_reactor<ndk::epoll_reactor>;
  if (mp->open(ndk::reactor_num_fixed, 2) != 0)
  {
    main_log->error("open multi reactor failed");
    return -1;
  }
  ndk::reactor::instance(new ndk::reactor(mp));
#endif
  g_acceptor = new ndk::acceptor<recv_data>(ndk::reactor::instance());
  ndk::inet_addr local_addr(8000);
  if (g_acceptor->open(local_addr) != 0)
  {
    main_log->error("open acceptor failed");
    return -1;
  }
#ifndef use_multi_reactor
  ndk::reactor::instance()->run_reactor_event_loop();
#else
  ndk::thread_manager::instance()->wait();
#endif
  return 0;
}

