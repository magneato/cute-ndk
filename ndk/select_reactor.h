// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 02:24
 */
//========================================================================

#ifndef NDK_SELECT_REACTOR_H_
#define NDK_SELECT_REACTOR_H_

#include <sys/select.h>

#include "ndk/unix_reactor.h"
#include "ndk/reactor_impl.h"

namespace ndk
{
  /**
   * @class select_handle
   *
   * @brief for list.
   */
  class select_handle
  {
  public:
    select_handle()
      : handle(NDK_INVALID_HANDLE), 
      next(0)
    {  }

    //
    inline void reset() { this->handle = NDK_INVALID_HANDLE; this->next = 0; }

    //
    ndk_handle handle;
    select_handle *next;
  };
  /**
   * @class select_reactor
   *
   * @brief 
   */
  class select_reactor_base : public unix_reactor
  {
  protected:
    select_reactor_base();

    //
    int open_i(timer_queue *tq);

    //
    int close_i(void);

    int reset_fd_set(fd_set *&rd_set, fd_set *&wr_set, fd_set *&ex_set);

    virtual int poll_events(const time_value *);

    //
    int dispatch_io_events();

    //
    int append_handle(ndk_handle h, select_handle *&sh);

    int remove_handle(ndk_handle h, select_handle *&sh);

    //
    int reactor_mask_to_select_event(ndk_handle h, reactor_mask mask);

    //
    int handle_opt_i(ndk_handle handle, reactor_mask mask, int opt);

    int find_handle(ndk_handle handle, select_handle *sh);

    inline select_handle *alloc_handle();

    inline void release_handle(select_handle *h);
  protected:
    bool initialized_;

    select_handle *handle_list_rd_;
    select_handle *handle_list_wr_;
    select_handle *handle_list_ex_;

    select_handle *free_node_;
    //
    fd_set *rd_set_;
    fd_set *wr_set_;
    fd_set *ex_set_;
  };
  /**
   * @class select_reactor
   *
   * @brief A `select' based reactor implemenatation.
   *
   */
  template<typename TOKEN> class select_reactor;

  // Thread-safe reactor.
  template<>
  class select_reactor<reactor_token> : public select_reactor_base
  {
  public:
    select_reactor() 
      : select_reactor_base(),
        token_(this)
    { }

    virtual ~select_reactor() { this->close(); }

    //
    virtual int open(timer_queue *tq = 0);

    virtual int close();

    // ++ Event loop drivers
    /**
     * This event loop driver blocks for up to @a max_wait_time before
     * returning. It will return earlier if events occur.  Note that
     * @a max_wait_time can be -1, in which case this method blocks
     * indefinitely until events occur.
     *
     * return  The total number of event_handlers that were dispatched,
     * 0 if the @a max_wait_time elapsed without dispatching any handlers, 
     * or -1 if an error occurs.
     */
    virtual int handle_events(const time_value *max_wait_time = 0);

    // ++ Register and remove Handlers. These method maybe called in user thread 
    // context, so must use mutex
    /**
     * Register event_handler with mask.  The I/O handle will always come 
     * from handle on the event_handler.
     */
    virtual int register_handler(event_handler *eh,
                                 reactor_mask mask);

    /**
     * Register event_handler with mask.  The I/O handle is provided through 
     * the io_handle parameter.
     */
    virtual int register_handler(ndk_handle io_handle, 
                                 event_handler *eh,
                                 reactor_mask mask);

    /**
     * The I/O handle will be obtained using handle() method of event_handler.
     * If mask == event_handler::DONT_CALL then the handle_close() 
     * method of the event_handler is not invoked.
     */
    virtual int remove_handler(event_handler *eh,
                               reactor_mask mask);

    /**
     * Removes <handle>.  If <mask> == <event_handler::DONT_CALL then the 
     * handle_close() method of the associated <event_handler> is not invoked.
     */
    virtual int remove_handler(ndk_handle io_handle,
                               reactor_mask mask);

    // ++ Timer management.

    /**
     * Schedule a timer event.
     * Schedule a timer event that will expire after an <delay> amount
     * of time.  The return value of this method, a timer_id value,
     * uniquely identifies the <eh> in the Reactor's internal
     * list of timers.  This timer_id value can be used to cancel the 
     * timer with the cancel_timer() call. 
     */
    virtual int schedule_timer(event_handler *eh,
                               const void *arg,
                               const time_value &delay,
                               const time_value &interval = time_value::zero);

    // unix-like cron timer.
    virtual int crontab(const event_handler *eh, 
                        const void *arg, 
                        const char *entry);

    /**
     * Resets the crontab-entry of the timer represented by <timer_id> to
     * <entry>.  Returns 0 if successful, -1 if not.
     */
    virtual int reset_crontab(int timer_id, const char *entry);

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
    virtual int cancel_timer(int timer_id,
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
    virtual int cancel_timer(event_handler *eh,
                             int dont_call_handle_close = 1);

    // Get token.
    virtual token &lock();
  protected:
    // L/F Pattern mutex
    reactor_token token_;
  };
  // Thread-unsafe reactor.
  template<>
  class select_reactor<reactor_null_token> : public select_reactor_base
  {
  public:
    select_reactor()
      : select_reactor_base(),
        token_(this)
    { }

    virtual ~select_reactor() { this->close(); }

    //
    virtual int open(timer_queue *tq = 0);

    virtual int close();

    // ++ Event loop drivers
    /**
     * This event loop driver blocks for up to @a max_wait_time before
     * returning. It will return earlier if events occur.  Note that
     * @a max_wait_time can be -1, in which case this method blocks
     * indefinitely until events occur.
     *
     * return  The total number of event_handlers that were dispatched,
     * 0 if the @a max_wait_time elapsed without dispatching any handlers, 
     * or -1 if an error occurs.
     */
    virtual int handle_events(const time_value *max_wait_time = 0);

    // ++ Register and remove Handlers. These method maybe called in user thread 
    // context, so must use mutex
    /**
     * Register event_handler with mask.  The I/O handle will always come 
     * from handle on the event_handler.
     */
    virtual int register_handler(event_handler *eh,
                                 reactor_mask mask);

    /**
     * Register event_handler with mask.  The I/O handle is provided through 
     * the io_handle parameter.
     */
    virtual int register_handler(ndk_handle io_handle, 
                                 event_handler *eh,
                                 reactor_mask mask);

    /**
     * The I/O handle will be obtained using handle() method of event_handler.
     * If mask == event_handler::DONT_CALL then the handle_close() 
     * method of the event_handler is not invoked.
     */
    virtual int remove_handler(event_handler *eh,
                               reactor_mask mask);

    /**
     * Removes <handle>.  If <mask> == <event_handler::DONT_CALL then the 
     * handle_close() method of the associated <event_handler> is not invoked.
     */
    virtual int remove_handler(ndk_handle io_handle,
                               reactor_mask mask);

    // ++ Timer management.

    /**
     * Schedule a timer event.
     * Schedule a timer event that will expire after an <delay> amount
     * of time.  The return value of this method, a timer_id value,
     * uniquely identifies the <eh> in the Reactor's internal
     * list of timers.  This timer_id value can be used to cancel the 
     * timer with the cancel_timer() call. 
     */
    virtual int schedule_timer(event_handler *eh,
                               const void *arg,
                               const time_value &delay,
                               const time_value &interval = time_value::zero);

    // unix-like cron timer.
    virtual int crontab(const event_handler *eh, 
                        const void *arg, 
                        const char *entry);

    /**
     * Resets the crontab-entry of the timer represented by <timer_id> to
     * <entry>.  Returns 0 if successful, -1 if not.
     */
    virtual int reset_crontab(int timer_id, const char *entry);

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
    virtual int cancel_timer(int timer_id,
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
    virtual int cancel_timer(event_handler *eh,
                             int dont_call_handle_close = 1);

    // Get token.
    virtual token &lock();
  protected:
    reactor_null_token token_;
  };

} // namespace ndk

#include "ndk/select_reactor.inl"
#endif // NDK_SELECT_REACTOR_H_

