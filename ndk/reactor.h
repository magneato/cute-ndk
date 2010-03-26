// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 02:24
 */
//========================================================================

#ifndef NDK_REACTOR_H_
#define NDK_REACTOR_H_

#include "ndk/types.h"
#include "ndk/guard.h"
#include "ndk/config.h"
#include "ndk/singleton.h"
#include "ndk/time_value.h"
#include "ndk/thread_mutex.h"
#include "ndk/reactor_impl.h"
#include "ndk/event_handler.h"

namespace ndk
{
  // Forward declarations
  //
  class reactor_impl;
  class event_handler;
  class token;

  /**
   * @class reactor
   *
   * @brief 
   */
  class reactor
  {
    friend class singleton<reactor, thread_mutex>;
  public:
    /**
     * Create the reactor using @a implementation.  The flag
     * @a delete_implementation tells the reactor whether or not to
     * delete the @a implementation on destruction.
     */
    reactor(reactor_impl* implementation = 0, 
            int delete_implementation = 0);
    /**
     * Close down and release all resources.
     * Any notifications that remain queued on this reactor instance
     * are lost 
     */
    ~reactor();

    // Get pointer to a process-wide Reactor.
    static reactor *instance(void);

    /**
     * Set pointer to a process-wide reactor and return existing
     * pointer.  If <delete_reactor> != 0 then we'll delete the 
     * reactor at destruction time.
     */
    static reactor *instance(reactor *r);

    // Delete the dynamically allocated Singleton
    static void close_singleton(int delete_reactor = 0);

    // ++ reactor event loop management methods.
    // These methods work with an instance of a reactor.
    /**
     * Run the event loop until the 
     * <Reactor::handle_events/Reactor::alertable_handle_events>
     * method returns -1 or the <end_reactor_event_loop> method is invoked.
     */
    int run_reactor_event_loop();

    /**
     * This event loop driver blocks for up to <max_wait_time> before
     * returning.  It will return earlier if events occur.  Note that
     * <max_wait_time> can be 0, in which case this method blocks
     * indefinitely until events occur.
     */
    int handle_events(const time_value *max_wait_time = 0);

    // ++ Register and remove handlers.
    /**
     * Register handler for I/O events.
     * The handle will come from event_handler::handle().
     *
     * If this handler/handle pair has already been registered, any 
     * new masks specified will be added.
     *
     * The reactor of the <eh> will be assigned to *this*
     * automatic, restore the original reactor if register failed
     */
    int register_handler(event_handler *eh,
                         reactor_mask mask);

    /**
     * Register handler for I/O events.
     * Same as register_handler(event_handler*, reactor_mask),
     * except handle is explicitly specified.
     */
    int register_handler(ndk_handle io_handle,
                         event_handler *eh,
                         reactor_mask mask);

    /**
     * Remove <masks> from <handle> registration.
     * For I/O handles, <masks> are removed from the Reactor.  Unless
     * <masks> includes <event_handler::DONT_CALL>, 
     * event_handler::handle_close() will be called with the <masks>
     * that have been removed. 
     */
    int remove_handler(ndk_handle handle, 
                       reactor_mask masks);

    /**
     * Remove <masks> from <eh> registration.
     * Same as remove_handler(ndk_handle, reactor_mask), except
     * <handle> comes from event_handler::handle().
     */
    int remove_handler(event_handler *eh,
                       reactor_mask masks);

    // ++ Timer management.
    /**
     * Schedule a timer that will expire after an <delay> amount
     * of time.  The return value of this method, a timer_id value,
     * uniquely identifies the <eh> in the Reactor's internal
     * list of timers.  This timer_id value can be used to cancel the 
     * timer with the cancel_timer() call. 
     */
    int schedule_timer(event_handler *eh,
                       const void *arg,
                       const time_value &delay,
                       const time_value &interval = time_value::zero);

    // Usage please refe to 'ndk/timer_queue.h'
    int crontab(const event_handler *eh, 
                const void *arg, 
                const char *entry);

    /**
     * Resets the crontab-entry of the timer represented by <timer_id> to
     * <entry>.  Returns 0 if successful, -1 if not.
     */
    int reset_crontab(int timer_id, const char *entry);

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
    int reset_timer_interval(int timer_id,
                             const time_value &interval);

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
    int cancel_timer(int timer_id,
                     const void **arg = 0,
                     int dont_call_handle_close = 1);

    /**
     * Cancel all timers associated with event handler.
     *
     * Shorthand for calling cancel_timer(long,const void **,int) 
     * multiple times for all timer associated with <eh>.
     *
     * event_handler::handle_close() will be called when 
     * <dont_call_handle_close> is zero, but not pass the argument 
     * passed in when the handler was registered, because it could 
     * register multiple argument<arg>, I don't know which should be
     * passed.
     *
     * Returns number of handlers cancelled.
     */
    int cancel_timer(event_handler *eh,
                     int dont_call_handle_close = 1);

    /**
     * Dispatch user specified events.
     *
     * Handler will be dispatched irrespective of whether it is
     * registered, not registered, or suspended in the Reactor.
     */
    int notify(event_handler *eh, void *msg);

    /**
     * Purge any notifications pending in this reactor for the specified
     * event_handler object. If eh == 0, all notifications for
     * all handlers are removed (but not any notifications posted just
     * to wake up the reactor itself). Returns the number of
     * notifications purged.  Returns -1 on error.
     */
    int purge_pending_notifications(event_handler *eh = 0,
                                    reactor_mask mask = event_handler::all_events_mask);

    // 
    int close(void);

    // Get token.
    token &lock();

    // Get the implementation class
    reactor_impl* implementation();

  protected:
    // Delegation/implementation class that all methods will be
    // forwarded to.
    reactor_impl *reactor_impl_;

    // Flag used to indicate whether we are responsible for cleaning up
    // the implementation instance
    int delete_impl_;

    // Pointer to a process-wide reactor singleton.
    static reactor *reactor_;
    static thread_mutex instance_lock_;
  private:
    // Deny access since member-wise won't work...
    reactor(const reactor &);
    reactor &operator =(const reactor &);
  };
} // namespace ndk

#include "ndk/reactor.inl"
#endif // NDK_REACTOR_H_

