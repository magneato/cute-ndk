// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 02:24
 */
//========================================================================

#ifndef NDK_ACCEPTOR_H_
#define NDK_ACCEPTOR_H_

#include "ndk/inet_addr.h"
#include "ndk/sock_stream.h"
#include "ndk/event_handler.h"

/**
 * @class acceptor

 * @brief 
 */
namespace ndk
{
  template<class svc_handler>
  class acceptor : public event_handler
  {
  public:
    acceptor();

    virtual ~acceptor();

    /**
     * Begin to listening, and register with the specified reactor 
     * for accept events. An acceptor can only listen to one port 
     * at a time, so make sure to close() the acceptor before calling
     * open() again.
     *
     */
    virtual int open(const inet_addr &local_addr,
                     reactor *r,
                     int reuse_addr = 1,
                     size_t rcvbuf_size = 0,
                     int protocol_family = AF_INET,
                     int backlog = NDK_DEFAULT_BACKLOG);

    // Returns the listening acceptor's {ndk_handle}.
    virtual inline ndk_handle get_handle(void) const;

    // Close down the Acceptor
    virtual inline int close(void);
  protected:
    // Accept a new connection and store remote addr to <remote_addr>.
    ndk_handle accept_i(inet_addr *remote_addr);

    // Create listen socket and bind to <local_addr>.
    int open_i(const inet_addr &local_addr,
               size_t rcvbuf_size,
               int reuse_addr,
               int protocol_family,
               int backlog);

    // = The following three methods define the Acceptor's strategies
    // for creating, accepting, and activating svc_handler's,
    // respectively

    /**
     * Bridge method for creating a svc_handler.  The default is to
     * create a new {svc_handler} if {sh} == 0, else {sh} is unchanged.
     * However, subclasses can override this policy to perform
     * svc_handler creation in any way that they like(such as creating
     * subclass instances of svc_handler, using a singleton, dynamically
     * linking the handler, etc.).  Returns -1 on failure, else 0.
     */
    virtual inline int make_svc_handler(svc_handler *&sh);

    /**
     * Bridge method for activating a {svc_handler} with the appropriate
     * concurrency strategy.  The default behavior of this method is to
     * activate the svc_handler by calling its {open} method(which
     * allows the svc_handler to define its own concurrency strategy).
     * However, subclasses can override this strategy to do more
     * sophisticated concurrency activations(such as making the
     * svc_handler as an "active object" via multi-threading).
     */
    virtual inline int activate_svc_handler(svc_handler *sh);

    // = Demultiplexing hooks.
    // Perform termination activities when {this} is removed from the
    // {reactor}.
    virtual inline int handle_close(ndk_handle = NDK_INVALID_HANDLE,
                             reactor_mask = event_handler::all_events_mask);

    // Accepts all pending connections from clients, and creates and
    // activates svc_handlers.
    virtual int handle_input(ndk_handle);
  private:
    // Shared handle.
    ndk_handle new_handle_;

    // For get new handle.
    socket listener_;

    // Store the new connection's remote addr.
    inet_addr shared_remote_addr_;
  };
} // namespace ndk

#include "ndk/acceptor.cpp"
#endif // NDK_ACCEPTOR_H_

