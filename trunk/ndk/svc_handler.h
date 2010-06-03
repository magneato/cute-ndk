// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-21 16:08
 */
//========================================================================

#ifndef NDK_SVC_HANDLER_H_
#define NDK_SVC_HANDLER_H_

#include "ndk/strace.h"
#include "ndk/reactor.h"
#include "ndk/sock_stream.h"
#include "ndk/event_handler.h"

namespace ndk
{
  /**
   * @class svc_handler
   * 
   * @brief
   */
  class svc_handler : public event_handler
  {
  public:
    /**
     * The <r> reactor is passed to event_handler
     */
    svc_handler()
      : closed_(0)
    { }

    //
    virtual ~svc_handler()
    {
      STRACE("");
      if (this->closed_ == 0)
      {
        this->closed_ = 1;
        this->shutdown();
      }
    }

    // Activate the client handler, this is typically 
    // called by acceptor or connector
    // The <arg> default is its generator which is 
    // acceptor or Connector
    virtual int open(void * = 0)
    { return -1; }

    /**
     * Object terminate hook -- application-specific cleanup
     * code goes here. Default action is call handle_close
     * This method maybe called by acceptor when accept an 
     * connection failed or connector.
     */
    virtual int close(int flag = 0)
    {
      STRACE("");
      return this->handle_close(NDK_INVALID_HANDLE, flag);
    }

    /**
     * Perform termination activites on the svc_handler.
     * The default behavior is to close down the <peer_>
     *(to avoid descriptor leaks) and to <destroy> this 
     * object(to avoid memory leaks)! If you don't want
     * this behavior make sure you override this method.
     */
    virtual int handle_close(ndk_handle = NDK_INVALID_HANDLE,
                             reactor_mask = event_handler::all_events_mask)
    {
      STRACE("");
      this->destroy();
      return 0;
    }

    // Returns the underlying sock_stream.  Used by Accpeotr
    // or Connector.
    sock_stream &peer() const
    { return(sock_stream &)this->peer_; }

    // Store new connection's remote address.
    virtual void set_remote_addr(const inet_addr &)
    { }

    // Get the underlying handle associated with the <peer_>.
    virtual ndk_handle get_handle() const
    { return this->peer_.get_handle(); }

    // Set the underlying handle associated with the <peer_>.
    virtual void set_handle(ndk_handle handle)
    { this->peer_.set_handle(handle); }

    /**
     * Call this to free up dynamically allocated <svc_handler>
     *(otherwise you will get memory leaks). In general, you 
     * should call this method rather than <delete> since 
     * this method knows whether or not the object was allocated
     * dynamically, and can act accordingly(i.e., deleting it if 
     * it was allocated dynamically).
     */
    virtual void destroy(void)
    {
      STRACE("");
      if (this->closed_ == 0)
      {
        // Will call the destructor, which automatically calls 
        // <shutdown>. 
        // <svc_handler> is allocate by Acceptor or Connector
        delete this;
      }
    }

    // Close down the descriptor and unregister from the reactor
    void shutdown(void)
    {
      STRACE("");
      if (this->get_reactor())
      {
        ndk::reactor_mask mask = event_handler::all_events_mask
          | event_handler::dont_call;

        this->get_reactor()->cancel_timer(this);

        if (this->peer().get_handle() != NDK_INVALID_HANDLE)
          this->get_reactor()->remove_handler(this, mask);
      }
      this->peer().close();
    }
  protected:
    // Maintain connection with client
    sock_stream peer_;

    // Keeps track of whether we are in the process of closing 
    //(required to avoid circular calls to <handle_close>).
    volatile int closed_;
  };
} // namespace ndk

#endif // NDK_SVC_HANDLER_H_

