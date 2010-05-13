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
#include "ndk/epoll_reactor.h"
#include "ndk/event_handler.h"

namespace ndk
{
  /**
   * @class reactor
   *
   * @brief 
   */
  class reactor : public singleton<reactor, thread_mutex>
  {
  public:
    /**
     * Create the reactor using @a implementation.  The flag
     * @a delete_implementation tells the reactor whether or not to
     * delete the @a implementation on destruction.
     */
    reactor(reactor_impl* impl= 0, int delete_impl= 0)
      : reactor_impl_(impl),
      delete_impl_(delete_impl)
    {
      if (this->reactor_impl_ == 0)
      {
        /**
         * epoll reactor is default reactor in linux platfrom.
         */
        this->reactor_impl_ = new epoll_reactor<reactor_null_token>();
        this->delete_impl_  = 1;
      }
    }

    /**
     * Close down and release all resources.
     * Any notifications that remain queued on this reactor instance
     * are lost 
     */
    ~reactor()
    {
      if (this->reactor_impl_ && this->delete_impl_)
        delete this->reactor_impl_;
      this->reactor_impl_ = 0;
    }

    // ++ reactor event loop management methods.
    // These methods work with an instance of a reactor.
    /**
     * Run the event loop until the 
     * <reactor::handle_events> method returns -1 or the 
     * <end_reactor_event_loop> method is invoked.
     */
    inline int run_reactor_event_loop()
    {
      while (1)
      {
        if (this->reactor_impl_->handle_events() == -1)
          break;
      }
      return -1;
    }

    /**
     * This event loop driver blocks for up to <max_wait_time> before
     * returning.  It will return earlier if events occur.  Note that
     * <max_wait_time> can be 0, in which case this method blocks
     * indefinitely until events occur.
     */
    inline int handle_events(const time_value *max_wait_time = 0)
    {
      return this->reactor_impl_->handle_events(max_wait_time);
    }

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
    inline int register_handler(event_handler *eh,
                                reactor_mask mask)
    {
      reactor *old_r = eh->get_reactor();
      eh->set_reactor(this);
      int result = this->reactor_impl_->register_handler(eh,
                                                         mask);
      if (result == -1)
        eh->set_reactor(old_r);
      return result;
    }

    /**
     * Register handler for I/O events.
     * Same as register_handler(event_handler*, reactor_mask),
     * except handle is explicitly specified.
     */
    int register_handler(ndk_handle handle,
                         event_handler *eh,
                         reactor_mask mask)
    {
      reactor *old_r = eh->get_reactor();
      eh->set_reactor(this);
      int result = this->reactor_impl_->register_handler(handle,
                                                         eh,
                                                         mask);
      if (result == -1)
        eh->set_reactor(old_r);
      return result;
    }
    /**
     * Remove <masks> from <handle> registration.
     * For I/O handles, <masks> are removed from the Reactor.  Unless
     * <masks> includes <event_handler::DONT_CALL>, 
     * event_handler::handle_close() will be called with the <masks>
     * that have been removed. 
     */
    inline int remove_handler(ndk_handle handle, 
                              reactor_mask mask)
    { return this->reactor_impl_->remove_handler(handle, mask); }

    /**
     * Remove <masks> from <eh> registration.
     * Same as remove_handler(ndk_handle, reactor_mask), except
     * <handle> comes from event_handler::handle().
     */
    inline int remove_handler(event_handler *eh,
                              reactor_mask mask)
    { return this->reactor_impl_->remove_handler(eh, mask); }

    // ++ Timer management.
    /**
     * Schedule a timer that will expire after an <delay> amount
     * of time.  The return value of this method, a timer_id value,
     * uniquely identifies the <eh> in the Reactor's internal
     * list of timers.  This timer_id value can be used to cancel the 
     * timer with the cancel_timer() call. 
     */
    inline int schedule_timer(event_handler *eh,
                       const void *arg,
                       const time_value &delay,
                       const time_value &interval = time_value::zero)
    {
      return this->reactor_impl_->schedule_timer(eh,
                                                 arg,
                                                 delay,
                                                 interval);
    }

    // Usage please refe to 'ndk/timer_queue.h'
    inline int crontab(const event_handler *eh, 
                const void *arg, 
                const char *entry)
    { return this->reactor_impl_->crontab(eh, arg, entry); }

    /**
     * Resets the crontab-entry of the timer represented by <timer_id> to
     * <entry>.  Returns 0 if successful, -1 if not.
     */
    inline int reset_crontab(int timer_id, const char *entry)
    { return this->reactor_impl_->reset_crontab(timer_id, entry); }

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
    inline int reset_timer_interval(int timer_id,
                             const time_value &interval)
    {
      return this->reactor_impl_->reset_timer_interval(timer_id,
                                                       interval);
    }

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
    inline int cancel_timer(int timer_id,
                     const void **arg = 0,
                     int dont_call_handle_close = 1)
    {
      return this->reactor_impl_->cancel_timer(timer_id,
                                               arg,
                                               dont_call_handle_close);
    }

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
    inline int cancel_timer(event_handler *eh,
                            int dont_call_handle_close = 1)
    {
      return this->reactor_impl_->cancel_timer(eh,
                                               dont_call_handle_close);
    }

    /**
     * Dispatch user specified events.
     *
     * Handler will be dispatched irrespective of whether it is
     * registered, not registered, or suspended in the Reactor.
     */
    inline int notify(event_handler *eh, void *msg)
    { return this->reactor_impl_->notify(eh, msg); }

    /**
     * Purge any notifications pending in this reactor for the specified
     * event_handler object. If eh == 0, all notifications for
     * all handlers are removed (but not any notifications posted just
     * to wake up the reactor itself). Returns the number of
     * notifications purged.  Returns -1 on error.
     */
    inline int purge_pending_notifications(event_handler *eh = 0,
                                           reactor_mask mask = event_handler::all_events_mask)
    { return this->reactor_impl_->purge_pending_notifications(eh, mask); }

    // 
    inline int close(void)
    { return this->reactor_impl_->close(); }

    // Get token.
    inline token &lock()
    { return this->reactor_impl_->lock(); }

    // Get the implementation class
    reactor_impl* implementation();

#ifdef NDK_DUMP
    inline void dump()
    { this->reactor_impl_->dump(); }
#endif
  protected:
    // Delegation/implementation class that all methods will be
    // forwarded to.
    reactor_impl *reactor_impl_;

    // Flag used to indicate whether we are responsible for cleaning up
    // the implementation instance
    int delete_impl_;
  private:
    // Deny access since member-wise won't work...
    reactor(const reactor &);
    reactor &operator =(const reactor &);
  };
} // namespace ndk

#endif // NDK_REACTOR_H_

