// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 02:24
 */
//========================================================================

#ifndef NDK_REACTOR_IMPL_H_
#define NDK_REACTOR_IMPL_H_

#include "ndk/event_handler.h"

namespace ndk
{
  /** 
   * @class notification_buffer
   *
   * @brief 
   */
  class reactor_impl;
  class token;
  /** 
   * @class reactor_notify
   *
   * @brief Abstract class for unblocking an reactor_impl from its event loop.
   */
  class reactor_notify : public event_handler
  {
  public:
    // = Initialization and termination methods.
    virtual int open(void *) = 0;

    virtual int close(void) = 0;

    /**
     * Called by a thread when it wants to unblock the <reactor_impl>.
     * This wakeups the <reactor_impl> if currently blocked.  Pass over
     * both the <event_handler> *and* the <msg> to allow the caller to
     * dictate which <event_handler::handle_msg> method the <reactor_impl>
     * will invoke.
     */
    virtual int notify(event_handler *eh, void *msg) = 0;

    // Returns the ndk_handle of the notify pipe on which the reactor
    // is listening for notifications so that other threads can unblock
    // the <reactor_impl>.
    virtual ndk_handle notify_handle(void) = 0;

    // Purge any notifications pending in this reactor for the specified
    // event_handler object. Returns the number of notifications purged. 
    // Returns -1 on error.
    virtual int purge_pending_notifications(event_handler * = 0,
                                            reactor_mask = event_handler::all_events_mask) = 0;
#ifdef NDK_DUMP
    virtual void dump() = 0;
#endif
  };
  /**
   * @class reactor_impl
   *
   * @brief 
   */
  class reactor_impl
  {
  public:
    reactor_impl()
    { }

    virtual ~reactor_impl()
    { }

    virtual int close(void) = 0;

    /**
     * This event loop driver blocks for up to <max_wait_time> before
     * returning.  It will return earlier if events occur.  Note that
     * <max_wait_time> can be 0, in which case this method blocks
     * indefinitely until events occur.
     *
     * Returns the total number of event_handlers that were
     * dispatched, 0 if the <max_wait_time> elapsed without dispatching
     * any handlers, or -1 if an error occurs.
     */
    virtual int handle_events(const time_value *max_wait_time = 0) = 0;

    // ++ Register and remove Handlers.
    /**
     * Register <event_handler> with <mask>.  The I/O handle will always
     * come from <handle> on the <event_handler>.
     */
    virtual int register_handler(event_handler *eh,
                                 reactor_mask mask) = 0;

    // Register <event_handler> with <mask>.
    virtual int register_handler(ndk_handle io_handle,
                                 event_handler *eh,
                                 reactor_mask mask) = 0;

    /**
     * Removes <event_handler>.  Note that the I/O handle will be
     * obtained using <handle> method of <event_handler> .  If
     * <mask> == <event_handler::dont_call> then the <handle_close>
     * method of the <event_handler> is not invoked.
     */
    virtual int remove_handler(event_handler *eh,
                               reactor_mask mask) = 0;

    /**
     * Removes <handle>.  If <mask> == <event_handler::dont_call>
     * then the <handle_close> method of the associated <event_handler>
     * is not invoked.
     */
    virtual int remove_handler(ndk_handle handle,
                               reactor_mask mask) = 0;

    // ++ Timer management.

    /**
     * Schedule a timer event.
     * Schedule a timer event that will expire after an <delay> amount
     * of time.  The return value of this method, a timer_id value,
     * uniquely identifies the <event_handler> in the Reactor's internal
     * list of timers.  This timer_id value can be used to cancel the 
     * timer with the cancel_timer() call. 
     */
    virtual int schedule_timer(event_handler *eh,
                               const void *arg,
                               const time_value &delay,
                               const time_value &interval = time_value::zero) = 0;

    /**
     * Reset recurring timer interval.
     *
     * Resets the interval of the timer represented by  timer_id to
     * interval, which is specified in relative time to the current
     * <gettimeofday>.  If <interval> is equal to time_value::zero,
     * the timer will become a non-rescheduling timer.  Returns 0 
     * if successful, -1 if not.
     * This change will not take effect until the next timeout.
     */
    virtual int reset_timer_interval(int timer_id,
                                     const time_value &interval) = 0;

    // Usage please refe to 'ndk/timer_queue.h'
    virtual int crontab(const event_handler *eh, 
                        const void *arg, 
                        const char *entry) = 0;

    /**
     * Resets the crontab-entry of the timer represented by <timer_id> to
     * <entry>.  Returns 0 if successful, -1 if not.
     */
    virtual int reset_crontab(int timer_id, const char *entry) = 0;

    /**
     * Cancel timer.
     *
     * Cancel timer associated with timer_id that was returned from
     * the schedule_timer() method.  If arg is non-NULL then it will be
     * set to point to the argument passed in when the handler was 
     * registered. This makes it possible to free up the memory and 
     * avoid memory leaks.  Returns 0 if cancellation succeeded and -1 
     * if the  timer_id wasn't found.
     *
     * On successful cancellation, event_handler::handle_close() will 
     * be called when <dont_call_handle_close> is zero, handle_close() 
     * will pass the argument passed in when the handler was 
     * registered, This makes it possible to release the memory of 
     * the <arg> in handle_close().
     * avoid memory leaks, also pass the argument <event_handler::TIMER_MASK>. 
     */
    virtual int cancel_timer(int timer_id,
                             const void **arg = 0,
                             int dont_call_handle_close = 1) = 0;

    /**
     * Cancel all timers associated with event handler.
     *
     * Shorthand for calling cancel_timer(long,const void **,int) 
     * multiple times for all timer associated with <event_handler>.
     *
     * event_handler::handle_close() will be called when 
     * <dont_call_handle_close> is zero, but not pass the argument 
     * passed in when the handler was registered, because it could 
     * register multiple argument<arg>, I don't know which should be
     * passed.
     *
     * Returns number of handlers cancelled.
     */
    virtual int cancel_timer(event_handler *eh,
                             int dont_call_handle_close = 1) = 0;
    /**
     * Dispatch user specified events.
     *
     * Handler will be dispatched irrespective of whether it is
     * registered, not registered, or suspended in the Reactor.
     */
    virtual int notify(event_handler *eh, void *msg) = 0;

     /**
      * Purge any notifications pending in this reactor for the specified
      * event_handler object. If eh == 0, all notifications for
      * all handlers are removed (but not any notifications posted just
      * to wake up the reactor itself). Returns the number of
      * notifications purged.  Returns -1 on error.
      */
    virtual int purge_pending_notifications(event_handler *eh = 0,
                                            reactor_mask mask = event_handler::all_events_mask) = 0;

    // Get token.
    virtual token &lock() = 0;
#ifdef NDK_DUMP
    virtual void dump() = 0;
#endif
  };
} // namespace ndk

#endif // NDK_REACTOR_IMPL_H_

