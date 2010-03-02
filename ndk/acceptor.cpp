#ifndef NDK_ACCEPTOR_CPP_
#define NDK_ACCEPTOR_CPP_

#include "ndk/strace.h"
#include "ndk/acceptor.h"
#include "ndk/reactor.h"

namespace ndk
{
template<class svc_handler>
acceptor<svc_handler>::acceptor()
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
  return this->peer_acceptor_.get_handle();
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
  if (this->peer_acceptor_.open(local_addr, 
                                rcvbuf_size,
                                reuse_addr,
                                protocol_family,
                                backlog) == -1)
    return -1;

  // Set the peer acceptor's handle into non-blocking mode.  This is a
  // safe-guard against the race condition that can otherwise occur
  // between the time when <select> indicates that a passive-mode
  // socket handle is "ready" and when we call <accept>.  During this
  // interval, the client can shutdown the connection, in which case,
  // the <accept> call can hang!
  ndk::set_non_block_mode(this->peer_acceptor_.get_handle());

  int result = r->register_handler(this, event_handler::accept_mask);
  if (result != -1)
    this->set_reactor(r);
  else
    this->peer_acceptor_.close();
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
    this->get_reactor()->remove_handler(this->get_handle(),
                                        event_handler::accept_mask | 
                                        event_handler::dont_call);
    this->set_reactor(0);
    // Shut down the listen socket to recycle the handles.
    if (this->peer_acceptor_.close() == -1)
      return -1;
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
int acceptor<svc_handler>::handle_input(ndk_handle listener)
{
  STRACE("");
  // Accept connections from clients. Note that a loop allows us to 
  // accept all pending connections without an extra trip through the 
  // Reactor and without having to use non-blocking I/O...
  do{
    //
    this->my_peer_.set_handle(NDK_INVALID_HANDLE);
    if (this->peer_acceptor_.accept(this->my_peer_,
                                    0, // remote addr
                                    0  // timeout 
                                   ) == -1)
      break;

    //
    svc_handler *sh = 0;
    if (this->make_svc_handler(sh) == -1)
      break;
    sh->peer().set_handle(this->my_peer_.get_handle());

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
} // namespace ndk

#endif // NDK_ACCEPTOR_CPP_


