// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-22 21:57
 * Brief    : For testing <epoll_reactor>, <connector>.
 */
//========================================================================

#include <ndk/connector.h>
#include <ndk/epoll_reactor.h>
#include <ndk/svc_handler.h>
#include <ndk/ndk_log.h>
#include <ndk/ndk_os.h>

#include <ndk/strace.h>

#include <deque>

static ndk::ndk_log *scanner_log = 
ndk::log_manager::instance()->get_logger("scanner_log");

int scann_count = 0;

ndk::thread_mutex g_conn_ports_list_mutex;
std::deque<int> g_conn_ports_list;

class port_scanner : public ndk::svc_handler
{
public:
  port_scanner(const char *ip, int port)
    : port_(port) 
  {
    strncpy(ip_, ip, sizeof(ip_));
    ip_[sizeof(ip_) - 1] = '\0';
  }
  ~port_scanner()
  {
    STRACE("");
    port_scanner::conn_count_++;
    if (port_scanner::conn_count_ >= scann_count)
    {
      //ndk::reactor::instance()->end_reactor_event_loop();
    }
  }
  //
  virtual int open(void *arg)
  {
    this->succ_count_++;
    g_conn_ports_list.push_back(this->port_);
    scanner_log->rinfo("connect port [%s:%d] successfully! [%d]", 
                       this->ip_,
                       this->port_,
                       port_scanner::succ_count_);
    if (ndk::reactor::instance()->register_handler(this, ndk::event_handler::read_mask) != 0)
        scanner_log->rinfo("register handle [%d] failed", this->get_handle());
    char s[] = "GET /xxx HTTP/1.1\r\nHost: 255.255.255.255:65535\r\n\r\n";
    this->peer().send(s, sizeof(s) - 1);
    return 0;
  }
  virtual int handle_input(ndk_handle)
  {
    char buff[4096] = {0};
    this->peer().recv(buff, sizeof(buff));
    scanner_log->rinfo("recb msg [%s]", buff);
    return -1;
  }
  virtual int handle_close(ndk::ndk_handle , ndk::reactor_mask mask)
  {
    if (mask == ndk::event_handler::timer_mask)
    {
      scanner_log->rinfo("connect port [%s:%d] timeout!", 
                         this->ip_,
                         this->port_);
    }else if (mask == ndk::event_handler::connect_mask)
    {
      scanner_log->rinfo("connect port [%s:%d] failed!", 
                         this->ip_,
                         this->port_);
    }else 
    {
      scanner_log->rinfo("handle [%d] mask", mask);
    }
    this->destroy();
    return 0;
  }
protected:
  int port_;

  char ip_[16];

  static int succ_count_;

  static int conn_count_;
};
int port_scanner::succ_count_ = 0;
int port_scanner::conn_count_ = 0;
ndk::connector<port_scanner> g_connector;

static ndk::ndk_log *main_log = ndk::log_manager::instance()->get_logger("main");

int main(int argc, char *argv[])
{
  if (argc != 4)
  {
    fprintf(stderr, "./port_scanner ip begin_port end_port\n");
    return 0;
  }
  char *ip = argv[1];
  int begin_port = ::atoi(argv[2]);
  int end_port   = ::atoi(argv[3]);
  if (ndk::log_manager::instance()->init("../logger_config.ini") != 0)
  {
    fprintf(stderr, "init logger failed\n");
    return 0;
  }
  ndk::epoll_reactor *ep = new ndk::epoll_reactor();
  if (ep->open() != 0)
  {
    main_log->error("open epoll reactor failed");
    return -1;
  }
  ndk::reactor::instance(new ndk::reactor(ep));
  
  g_connector.open(ndk::reactor::instance());

  scann_count = end_port - begin_port + 1;
  for (int port = begin_port; port <= end_port; ++port)
  {
    ndk::inet_addr remote_addr(port, ip);
    g_connector.connect(new port_scanner(ip, port),
                        remote_addr);
  }
  ndk::reactor::instance()->run_reactor_event_loop();
  std::deque<int>::iterator pos;
  printf("[");
  for (pos = g_conn_ports_list.begin();
       pos != g_conn_ports_list.end();
       ++pos)
  {
    printf("%d, ", *pos);
  }
  if (g_conn_ports_list.empty())
    printf("]\n");
  else
    printf("\b\b]\n");
  return 0;
}
