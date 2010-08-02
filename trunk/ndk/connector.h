// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 02:24
 */
//========================================================================

#ifndef NDK_CONNECTOR_H_
#define NDK_CONNECTOR_H_

#include "ndk/types.h"
#include "ndk/guard.h"
#include "ndk/token.h"
#include "ndk/strace.h"
#include "ndk/inet_addr.h"
#include "ndk/sock_stream.h"
#include "ndk/thread_mutex.h"
#include "ndk/event_handler.h"

#include <cassert>

namespace ndk
{
  /**
   * @class nonblocking_connect_handler
   *
   * @brief 
   */
  template<typename svc_handler_t> class connector;

  template<typename svc_handler_t>
  class nonblocking_connect_handler : public event_handler
  {
    friend class connector<svc_handler_t>;
  public:
    nonblocking_connect_handler(connector<svc_handler_t> &cn, svc_handler_t *h)
      : connector_(cn),
      svc_handler_(h),
      timer_id_(-1)
    { STRACE(""); }

    ~nonblocking_connect_handler()
    { STRACE(""); }

    inline void *operator new (size_t bytes)
    { return ::new char[bytes]; }

    inline void operator delete (void *ptr)
    { ::delete [] ((char *)ptr); }

    int close(svc_handler_t *&sh)
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

    inline svc_handler_t *svc_handler(void) 
    { return this->svc_handler_; }

    inline int  timer_id(void)    
    { return this->timer_id_; }

    inline void timer_id(int id) 
    { this->timer_id_ = id; }

    inline ndk_handle get_handle(void) const
    { return this->svc_handler_->get_handle(); }

    inline void set_handle(ndk_handle handle)
    { this->svc_handler_->set_handle(handle); }
    
    // called by reactor when asynchronous connections fail. 
    inline virtual int handle_input(ndk_handle )
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

    // called by reactor when asynchronous connections succeed. 
    inline virtual int handle_output(ndk_handle handle)
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

    // called by reactor when asynchronous connections succeed.(on
    // some platforms only). 
    inline virtual int handle_exception(ndk_handle )
    {
      STRACE("");
      // not support !
      assert(0);
      delete this; return -1;
    }

    // called if a connection times out before completing.
    inline virtual int handle_timeout(const void *,
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
    inline virtual int handle_close(ndk_handle , reactor_mask )
    {
      STRACE("");
      svc_handler_t *sh = 0;
      int result = this->close(sh);

      if (sh != 0)
        sh->close(event_handler::connect_mask);

      assert(result == 0);
      delete this; return 0;
    }
  private:
    // Associated timer id.
    int timer_id_;

    //
    connector<svc_handler_t> &connector_;

    //
    svc_handler_t *svc_handler_;
  };
  //
  /**
   * @class connector
   *
   * @brief 
   */
  template<typename svc_handler_t>
  class connector : public event_handler
  {
    friend class nonblocking_connect_handler<svc_handler_t>;
  public:
    connector()
    { }

    virtual ~connector()
    { }

    inline virtual int open(reactor *r)
    {
      this->set_reactor(r);
      return 0;
    }

    /**
     * Initiate connection of <svc_handler> to peer at <remote_addr> 
     * using synch_options.  If the caller wants to designate the
     * selected <local_addr> they can (and can also insist that the
     * <local_addr> be reused by passing a value <reuse_addr> ==
     * 1).If the connection fails the <close> hook on the <svc_handler> 
     * will be called automatically to prevent resource leaks.
     */
    virtual int connect(svc_handler_t *sh, 
                        const inet_addr &remote_addr, 
                        const time_value *timeout = 0,
                        const inet_addr &local_addr = inet_addr::addr_any,
                        const size_t rcvbuf_size = 0,
                        const int reuse_addr = 0)
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

  protected:
    int connect_i(sock_stream &new_stream,
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

      if (local_addr != inet_addr::addr_any)
      {
        sockaddr *laddr = reinterpret_cast<sockaddr *>(local_addr.get_addr());
        int size = local_addr.get_addr_size();
        if (::bind(new_stream.get_handle(), laddr, size) == -1)
        {
          new_stream.close();
          return -1;
        }
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

    int nonblocking_connect(svc_handler_t *sh, const time_value *timeout)
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

    void init_svc_handler(svc_handler_t *sh, ndk_handle handle)
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
  };
} // namespace ndk
#endif // NDK_CONNECTOR_H_

