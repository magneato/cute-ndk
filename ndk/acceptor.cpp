#ifndef NDK_ACCEPTOR_CPP_
#define NDK_ACCEPTOR_CPP_

#include "ndk/strace.h"
#include "ndk/acceptor.h"
#include "ndk/reactor.h"

namespace ndk
{
template<class svc_handler>
acceptor<svc_handler>::acceptor()
: new_handle_(NDK_INVALID_HANDLE)
{
}
template<class svc_handler>
acceptor<svc_handler>::~acceptor()
{
  this->handle_close();
}
template<class svc_handler>
inline ndk_handle acceptor<svc_handler>::get_handle() const
{
  return this->listener_.get_handle();
}
template<class svc_handler>
int acceptor<svc_handler>::open(const inet_addr &local_addr, 
                                reactor *r,
                                int reuse_addr/* = 1*/,
                                size_t rcvbuf_size/* = 0*/,
                                int protocol_family/* = AF_INET*/,
                                int backlog/* = NDK_DEFAULT_BACKLOG*/)
{
  STRACE("");
  if (r == 0) return -1;
  if (this->open_i(local_addr, 
                   rcvbuf_size,
                   reuse_addr,
                   protocol_family,
                   backlog) == -1)
    return -1;

  this->shared_remote_addr_.set_type(protocol_family);
  int result = r->register_handler(this, event_handler::accept_mask);
  if (result != -1)
    this->set_reactor(r);
  else
    this->listener_.close();
  return result;
}
template<class svc_handler>
inline int acceptor<svc_handler>::close()
{
  return this->handle_close();
}
template<class svc_handler>
int acceptor<svc_handler>::handle_close(ndk_handle /* = NDK_INVALID_HANDLE*/,
                                        reactor_mask /* = event_handler::ALL_EVENTS_MASK*/)
{
  STRACE("");
  if (this->get_reactor() != 0)
  {
    this->get_reactor()->remove_handler(this->listener_.get_handle(),
                                        event_handler::accept_mask | 
                                        event_handler::dont_call);
    this->set_reactor(0);
    // Shut down the listen socket to recycle the handles.
    this->listener_.close();
  }
  return 0;
}
template<class svc_handler>
inline int acceptor<svc_handler>::make_svc_handler(svc_handler *&sh)
{
  STRACE("");
  if (sh == 0)
  {
    sh = new svc_handler();
    if (sh == 0) return -1;
  }
  // Set the reactor of the newly created <svc_handler> to the same
  // reactor that this <acceptor> is using.
  sh->set_reactor(this->get_reactor());
  return 0;
}
template<class svc_handler>
inline int acceptor<svc_handler>::activate_svc_handler(svc_handler *sh)
{
  STRACE("");
  if (sh->open((void *)this) == -1)
  {
    sh->close(); return -1;
  }
  return 0;
}
template<class svc_handler>
int acceptor<svc_handler>::handle_input(ndk_handle )
{
  STRACE("");
  // Accept connections from clients. Note that a loop allows us to 
  // accept all pending connections without an extra trip through the 
  // Reactor and without having to use non-blocking I/O...
  do{
    //
    this->new_handle_ = this->accept_i(&(this->shared_remote_addr_));
    if (this->new_handle_ == NDK_INVALID_HANDLE)
      break;

    //
    svc_handler *sh = 0;
    if (this->make_svc_handler(sh) == -1)
      break;
    
    ndk::set_non_block_mode(this->new_handle_);
    sh->peer().set_handle(this->new_handle_);
    sh->set_remote_addr(this->shared_remote_addr_);

    // Activate the <svc_handler> using the designated concurrency
    // strategy
    this->activate_svc_handler(sh);
  }
#if 0  // If listener > FD_SETSIZE maybe core-down.
  // Now, check to see if there is another connection pending and
  // break out of the loop if there is none.
  while (ndk::handle_read_ready(listener, &time_value::zero) == 1);
#else
  while (1);
#endif

  return 0;
}
template<class svc_handler>
int acceptor<svc_handler>::open_i(const inet_addr &local_addr,
                                  size_t rcvbuf_size,
                                  int reuse_addr,
                                  int protocol_family,
                                  int backlog)
{
  if (this->listener_.open(SOCK_STREAM, protocol_family) == -1)
    return -1;

  int error = 0;
  if (rcvbuf_size != 0)
    socket::set_rcvbuf(this->listener_.get_handle(), rcvbuf_size);

  if (reuse_addr)
    socket::reuseaddr(this->listener_.get_handle(), 1);

  if (protocol_family == AF_INET)
  {
    sockaddr_in local_inet_addr;
    std::memset(reinterpret_cast<void *>(&local_inet_addr),
                0,
                sizeof(local_inet_addr));
    if (local_addr == inet_addr::addr_any)
      local_inet_addr.sin_port  = 0;
    else
      local_inet_addr = *reinterpret_cast<sockaddr_in *>(local_addr.get_addr());
    if (local_inet_addr.sin_port == 0)
    {
      if (ndk::bind_port(this->listener_.get_handle(), 
                         ntohl(local_inet_addr.sin_addr.s_addr), 
                         protocol_family) == -1)
        error = 1;
    }else if (::bind(this->listener_.get_handle(),
                     reinterpret_cast<sockaddr *>(&local_inet_addr),
                     sizeof(local_inet_addr)) == -1)
     error = 1;
    
  }else 
    error = 1;   // no support
  if (error != 0 || ::listen(this->listener_.get_handle(), backlog) == -1)
  {
    this->listener_.close(); return -1;
  }

  // Set the peer acceptor's handle into non-blocking mode.  This is a
  // safe-guard against the race condition that can otherwise occur
  // between the time when <select> indicates that a passive-mode
  // socket handle is "ready" and when we call <accept>.  During this
  // interval, the client can shutdown the connection, in which case,
  // the <accept> call can hang!
  this->listener_.set_nonblock();
  return 0;
}
template<class svc_handler>
ndk_handle acceptor<svc_handler>::accept_i(inet_addr *remote_addr)
{
  ndk_handle new_handle = NDK_INVALID_HANDLE;
  int len = remote_addr->get_addr_size();
  do
  {
    new_handle = ::accept(this->listener_.get_handle(), 
                          (sockaddr *)remote_addr->get_addr(), 
                          (socklen_t *)&len);
  }while (new_handle == NDK_INVALID_HANDLE
          && errno == EINTR);
  return new_handle;
}
} // namespace ndk

#endif // NDK_ACCEPTOR_CPP_


