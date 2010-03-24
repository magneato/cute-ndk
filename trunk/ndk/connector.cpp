#ifndef NDK_CONNECTOR_CPP_
#define NDK_CONNECTOR_CPP_

#include <cstdio>
#include <cassert>

#include "ndk/guard.h"
#include "ndk/token.h"
#include "ndk/connector.h"

namespace ndk
{
template<typename svc_handler_t>
nonblocking_connect_handler<svc_handler_t>::nonblocking_connect_handler(connector<svc_handler_t> &cn,
                                                                        svc_handler_t *h)
: connector_(cn),
  svc_handler_(h),
  timer_id_(-1)
{
  STRACE("");
}
template<typename svc_handler_t>
nonblocking_connect_handler<svc_handler_t>::~nonblocking_connect_handler()
{
  STRACE("");
}
template<typename svc_handler_t>
int nonblocking_connect_handler<svc_handler_t>::close(svc_handler_t *&sh)
{
  STRACE("");
  if (!this->svc_handler_) return -1;
  
  sh = this->svc_handler_;
  this->svc_handler_ = 0;
  // 1. cancel timer
  if (this->timer_id_ != -1)
  {
    if (this->get_reactor()->cancel_timer(this->timer_id_, 
                                          0,
                                          1) == -1)
      return -1;
    this->timer_id_ = -1;
  }

  // 3. remove handler from reactor.
  if (sh->get_handle() != NDK_INVALID_HANDLE)
  {
    if (this->get_reactor()->remove_handler(sh->get_handle(),
                                            ndk::event_handler::all_events_mask | 
                                            ndk::event_handler::dont_call) == -1)
      return -1;
  }
  return 0;
}
template<typename svc_handler_t>
inline int nonblocking_connect_handler<svc_handler_t>::handle_timeout(const void *, 
                                                                      const time_value &tv)
{
  STRACE("");
  // called if a connection times out before completing.
  
  this->timer_id_ = -1;

  svc_handler_t *sh = 0;
  int result = this->close(sh);

  if (sh != 0 && sh->handle_timeout(0, tv) == -1)
    sh->handle_close(sh->get_handle(), ndk::event_handler::timer_mask);

  assert(result == 0);
  delete this; return 0;
}
template<typename svc_handler_t>
inline int nonblocking_connect_handler<svc_handler_t>::handle_input(ndk_handle )
{
  STRACE("");
  // called by reactor when asynchronous connections fail. 

  svc_handler_t *sh = 0;
  int result = this->close(sh);

  if (sh != 0)
    sh->close(event_handler::connect_mask);

  assert(result == 0);
  delete this; return 0;
}
template<typename svc_handler_t> 
inline int nonblocking_connect_handler<svc_handler_t>::handle_output(ndk_handle handle)
{
  STRACE("");
  // connect successfully.
    
  svc_handler_t *sh = 0;
  int result = this->close(sh);

  if (sh != 0)
    this->connector_.init_svc_handler(sh, handle);
  assert(result == 0);
  delete this; return 0;
}
template<typename svc_handler_t>
inline int nonblocking_connect_handler<svc_handler_t>::handle_exception(ndk_handle )
{
  STRACE("");
  // not support !
  assert(0);
  delete this; return -1;
}
template<typename svc_handler_t>
inline int nonblocking_connect_handler<svc_handler_t>::handle_close(ndk_handle , 
                                                                    reactor_mask )
{
  STRACE("");
  svc_handler_t *sh = 0;
  int result = this->close(sh);

  if (sh != 0)
    sh->close(event_handler::connect_mask);

  assert(result == 0);
  delete this; return 0;
}
// ---------------------------------------------------------------
template<typename svc_handler_t>
int connector<svc_handler_t>::connect(svc_handler_t *sh,
                                      const inet_addr &remote_addr, 
                                      const time_value *timeout/* = 0*/,
                                      const inet_addr &local_addr/* = inet_addr::addr_any*/,
                                      const size_t rcvbuf_size/* = 0*/,
                                      const int reuse_addr/* = 0*/)
{
  STRACE("");
  if (sh == 0) return -1;

  sh->set_reactor(this->get_reactor());

  int result = this->connect_i(sh->peer(),
                               remote_addr,
                               local_addr,
                               AF_INET,
                               reuse_addr,
                               rcvbuf_size);

  // Activate immediately if we are connected.
  if (result == 0)
  {
    if (sh->open((void*)this) != 0)
    {
      sh->close(); return -1;
    }
  }else if (errno == EINPROGRESS)
  {
    if (timeout && *timeout == time_value::zero)
    {
      sh->close(event_handler::connect_mask);
      return -1;
    }
    result = this->nonblocking_connect(sh, timeout);
  }else
  {
    sh->close(event_handler::connect_mask);
  }
  return result;
}
template<typename svc_handler_t>
int connector<svc_handler_t>::nonblocking_connect(svc_handler_t *sh,
                                                  const time_value *timeout)
{
  STRACE("");
  guard<token> g(this->get_reactor()->lock());
  nonblocking_connect_handler<svc_handler_t> *nbch = 
    new nonblocking_connect_handler<svc_handler_t>(*this, sh);
  if (nbch == 0) return -1;

  int mask = event_handler::connect_mask;
  if (this->get_reactor()->register_handler(sh->get_handle(),
                                            nbch,
                                            mask) == -1)
  {
    assert(0);
    sh->close(mask);
    return -1;
  }

  if (timeout != 0)
  {
    int timer_id = this->get_reactor()->schedule_timer(nbch,
                                                       0,
                                                       *timeout);
    if (timer_id == -1)
    {
      assert(timer_id != -1);
      this->get_reactor()->remove_handler(sh->get_handle(), mask);
      sh->close(mask);
      return -1;
    }
    nbch->timer_id(timer_id);
  }

  return 0;
}
template<typename svc_handler_t>
void connector<svc_handler_t>::init_svc_handler(svc_handler_t *sh,
                                                ndk_handle handle)
{
  STRACE("");
  sh->set_handle(handle);
  inet_addr peer_addr;
  if (sh->peer().get_remote_addr(peer_addr) != -1)
  {
    if (sh->open((void*)this) == 0)
      return ;
  }
  sh->close(event_handler::connect_mask);
}
template<typename svc_handler_t>
int connector<svc_handler_t>::connect_i(sock_stream &new_stream,
                                        const inet_addr &remote_addr,
                                        const inet_addr &local_addr,
                                        int protocol_family,
                                        int reuse_addr,
                                        size_t recvbuf_size)
{
  if (new_stream.get_handle() == NDK_INVALID_HANDLE
      && new_stream.open(SOCK_STREAM, protocol_family) == -1)
    return -1;

  // all of sockets create by framwork is nonblocked.
  new_stream.set_nonblock();

  sockaddr *laddr = reinterpret_cast<sockaddr *>(local_addr.get_addr());
  int size = local_addr.get_addr_size();
  if (::bind(new_stream.get_handle(), laddr, size) == -1)
  {
    new_stream.close();
    return -1;
  }

  if (recvbuf_size != 0 &&
      socket::set_rcvbuf(new_stream.get_handle(), recvbuf_size) == -1)
  {
    new_stream.close();
    return -1;
  }
  if (reuse_addr)
    socket::reuseaddr(new_stream.get_handle(), 1);

  return ::connect(new_stream.get_handle(), 
                   reinterpret_cast<sockaddr *>(remote_addr.get_addr()),
                   remote_addr.get_addr_size());
}
} // namespace ndk
#endif  // NDK_CONNECTOR_CPP_
