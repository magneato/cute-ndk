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
#include "ndk/strace.h"
#include "ndk/inet_addr.h"
#include "ndk/sock_stream.h"
#include "ndk/thread_mutex.h"
#include "ndk/event_handler.h"

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
    nonblocking_connect_handler(connector<svc_handler_t> &cn, svc_handler_t *h);

    ~nonblocking_connect_handler();

    inline void *operator new (size_t bytes)
    { return ::new char[bytes]; }

    inline void operator delete (void *ptr)
    { ::delete [] ((char *)ptr); }

    int close(svc_handler_t *&);

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
    inline virtual int handle_input(ndk_handle handle);

    // called by reactor when asynchronous connections succeed. 
    inline virtual int handle_output(ndk_handle handle);

    // called by reactor when asynchronous connections succeed.(on
    // some platforms only). 
    inline virtual int handle_exception(ndk_handle handle);

    // called if a connection times out before completing.
    inline virtual int handle_timeout(const void *arg,
                                      const time_value &);

    inline virtual int handle_close(ndk_handle , reactor_mask );
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
                        const int reuse_addr = 0);

  protected:
    int connect_i(sock_stream &new_stream,
                  const inet_addr &remote_addr,
                  const inet_addr &local_addr,
                  int protocol_family,
                  int reuse_addr,
                  size_t recvbuf_size);

    int nonblocking_connect(svc_handler_t *sh, const time_value *timeout);

    void init_svc_handler(svc_handler_t *sh, ndk_handle handle);
  };
} // namespace ndk
#include "ndk/connector.cpp"
#endif // NDK_CONNECTOR_H_

