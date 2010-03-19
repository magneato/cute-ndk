// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-01-19 14:33
 */
//========================================================================

#ifndef NDK_UNIX_REACTOR_H_
#define NDK_UNIX_REACTOR_H_

#include "ndk/pipe.h"
#include "ndk/strace.h"
#include "ndk/timer_queue.h"
#include "ndk/reactor_impl.h"
#include "ndk/reactor_token.h"

namespace ndk
{
  /**
   * @class reactor_event_tuple
   *
   * @brief class that associates specific event mask with a given event 
   *        handler.
   */
  class reactor_event_tuple
  {
  public:
    reactor_event_tuple()
      : event_handler_(0),
      mask_(0)
    { }

    // The event handler
    event_handler *event_handler_;

    // The event mask for the above event handler.
    reactor_mask  mask_;
  };
  /**
   * class unix_reactor_handler_repository
   * @brief
   * Used to map ndk_handles onto the appropriate event_handler *.
   *
   * This class is simply a container that maps a handle to its
   * corresponding event handler.  It is not meant for use outside of.
   * the unix_reactor
   */
  class unix_reactor_handler_repository
  {
  public:
    unix_reactor_handler_repository()
      : max_size_(0),
      handlers_(0)
    { }

    // Initialize a repository of the appropriate size.
    int open(int size);

    // Close down the repository.
    int close(void);

    /**
     * Return the event_handler associated with ndk_handle.  If
     * index_p is non-zero, then return the index location of the handle, 
     * if found.
     */
    event_handler *find(ndk_handle handle, size_t *index_p = 0);

    // Bind the event_handler to handle with the appropriate reactor_mask 
    // setttings
    int bind(ndk_handle handle, event_handler *handler, reactor_mask mask);

    // Remove the binding for ndk_handle
    int unbind(ndk_handle handle);
    int unbind_all(void);

    // Check the handle to make sure it's a valid ndk_handle that is within 
    // the range of legal handles(i.e., greater than or equal to zero and 
    // less than max_size_).
    int is_invalid_handle(ndk_handle handle) const;

    // Check the handle to make sure it's a valid ndk_handle that is within 
    // the range of currently registered handles(i.e., greater than or 
    // equal to zero and less than max_handle_).
    int handle_in_range(ndk_handle handle) const;

    // Returns the current table size.
    size_t size(void) const;

    // Set the event mask for event handler associated with the given handle
    void mask(ndk_handle handle, reactor_mask mask); 

    // Retrieve the event mask for the event handler associated with the 
    // given handle.
    reactor_mask mask(ndk_handle handle);
  private:
    // Maximum number of handles.
    int max_size_;

    // The underlying array of event handlers.
    // The array of event handlers is directly indexed directly using 
    // an ndk_handle valie
    // This is Unix-specific.
    reactor_event_tuple *handlers_;
  }; // class unix_reactor_handler_repository

  /**
   * @class unix_reactor_notify
   *
   * @brief Event handler used for unblocking the unix_reactor from its event 
   * loop.
   *
   * This event handler is used internally by the unix_reactor as a means to 
   * allow a thread other then the one running the event loop to unblock the 
   * event loop.
   */
  class unix_reactor;
  class unix_reactor_notify : public reactor_notify
  {
  public:
    unix_reactor_notify();

    // = Initialization and termination methods.
    virtual int open(void *);

    virtual int close();

    /**
     * Called by a thread when it wants to unblock the <reactor_impl>.
     * This wakeups the <reactor_impl> if currently blocked.  Pass over
     * both the <event_handler> *and* the <mask> to allow the caller to
     * dictate which <event_handler> method the <reactor_impl> will invoke.
     */
    virtual int notify(event_handler *eh = 0,
                       reactor_mask mask = event_handler::except_mask);

    // Returns the ndk_handle of the notify pipe on which the reactor
    // is listening for notifications so that other threads can unblock
    // the <reactor_impl>.
    virtual ndk_handle notify_handle(void);

    // Read one of the notify call on the <handle> into the
    // <buffer>. This could be because of a thread trying to unblock
    // the <reactor_impl>
    virtual int read_notify_pipe(ndk_handle handle,
                                 notification_buffer &buffer);

    // Purge any notifications pending in this reactor for the specified
    // event_handler object. Returns the number of notifications purged. 
    // Returns -1 on error.
    virtual int purge_pending_notifications(event_handler * = 0,
                                            reactor_mask = event_handler::all_events_mask);

    // Handle readable event
    virtual int handle_input(ndk_handle handle);
  protected:
    // Keep a back pointer to the unix_reactor. 
    unix_reactor *reactor_impl_;

    /**
     * Contains the ndk_handle the unix_reactor is listening on, as well as
     * the ndk_handle that threads wanting the attention of the unix_reactor 
     * will write to.
     */
    pipe notification_pipe_;
  }; 
  /**
   * @class unix_reactor
   * 
   * @brief
   */
  class unix_reactor : public reactor_impl
  {
  public:
    enum
    {
      add_mask       = 0x11,
      set_mask       = 0x12,
      clr_mask       = 0x13
    };
    // 
    virtual ~unix_reactor() {}
  public:
    // 
    virtual int notify(event_handler *eh = 0,
                       reactor_mask mask = event_handler::except_mask)
    {
      return this->notify_handler_->notify(eh, mask);
    }

    // Purge any notifications pending in this reactor for the specified
    // event_handler object. Returns the number of notifications purged. 
    // Returns -1 on error.
    virtual int purge_pending_notifications(event_handler * eh = 0,
                                            reactor_mask mask = event_handler::all_events_mask)
    {
      return this->notify_handler_->purge_pending_notifications(eh, mask);
    }
  protected:
    unix_reactor();

    /**
     * Initialize unix_reactor with size.
     * Note On Unix platforms, the <size> parameter should be 
     * as large as the maximum number of file descriptors allowed 
     * for a given process.  This is necessary since a file descriptor
     * is used to directly index the array of event handlers
     * maintained by the Reactor's handler repository.  Direct
     * indexing is used for efficiency reasons.  If the size
     * parameter is less than the process maximum, the process
     * maximum will be decreased in order to prevent potential
     * access violations.
     * max_size_ whill be set to ::max_handles() if <size> equel to zero.
     */
    int open_ii(int size, timer_queue *tq); 

    // Close down and release all resources.
    int close_ii(void);

    /**
     * Poll for events and return the number of event handlers that
     * were dispatched.This is a helper method called by all handle_events() 
     * methods.
     */
    int handle_events_i(const time_value *max_wait_time);

    /**
     * Perform the upcall with the given event handler method.
     */
    int upcall(event_handler *event_handler,
               int(event_handler::*callback)(ndk_handle),
               ndk_handle handle);

    //
    int poll_events_i(const time_value *max_wait_time);

    //
    virtual int poll_events(const time_value *max_wait_time) = 0;

    /**
     * Dispatch event_handlers for time events, I/O events, Returns the 
     * total number of event_handlers that were dispatched or -1 if something 
     * goes wrong, 0 if no events ready.
     */
    int dispatch_events(void);

    /**
     * Dispatch all timers, if ready.
     * Returns: 0 if no timers ready(token still held),
     *		1 if a timer was expired(token released)
     */
    int dispatch_timer_handler(void);
    /**
     * Dispatch all IO events to the corresponding event handler.
     * Returns: 0 if no events ready ,
     *	       -1 on error
     *	       >0 the number of the ready events
     */
    virtual int dispatch_io_events(void) = 0;

    /**
     * Register the given event handler with the reactor.
     */
    int register_handler_i(ndk_handle handle,
                           event_handler *event_handler,
                           reactor_mask mask);

    // Remove the event handler which relate the handle
    int remove_handler_i(ndk_handle handle,
                         reactor_mask mask);

    virtual int handle_opt_i(ndk_handle handle,
                             reactor_mask mask,
                             int opt) = 0;
  protected:
    // The maximum number of file descriptors over which demultiplexing will occur.
    int size_;
    
    int active_fds_;  // for select.

    // Keeps track of whether we should delete the timer queue(if we
    // didn't create it, then we don't delete it).
    int delete_timer_queue_;

    // Timer queue
    timer_queue *timer_queue_;

    // Callback object that unblocks the <unix_reactor> if it's sleeping.
    reactor_notify *notify_handler_;

    // The repository that contains all registered event handlers.
    unix_reactor_handler_repository handler_rep_;
  };
} // namespace ndk

#include "ndk/unix_reactor.inl"
#endif // NDK_UNIX_REACTOR_H_

