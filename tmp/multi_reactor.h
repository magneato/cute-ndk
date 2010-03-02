// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-07-04 00:12
 */
//========================================================================

#ifndef NDK_MULTI_REACTOR_H_
#define NDK_MULTI_REACTOR_H_

#include "ndk/reactor_impl.h"

#if defined (NDK_WIN32)
# include<map>
#endif

namespace ndk
{
  template<typename REACTOR>
  class reactor_handle_tuple
  {
  public:
    reactor_handle_tuple()
      : reactor_(0),
      mask_(0)
    { }

    REACTOR *reactor_;

    // The event mask for the above event handler.
    reactor_mask mask_;
  };
  /**
   * @class multi_reactor_handle_repository
   *
   * @brief Used to map ndk_handles onto the appropriate reactor which
   * ndk_handle be registered in.
   */
  template<typename REACTOR>
  class multi_reactor_handle_repository
  {
  public:
    multi_reactor_handle_repository()
      : max_size_(0),
      reactor_handle_map_(0)
    { }

    int open(int max_size);

    int is_invalid_handle(ndk_handle handle);

    int handle_in_range(ndk_handle handle);

    REACTOR *find(ndk_handle handle);

    int bind(ndk_handle handle, REACTOR *r, reactor_mask mask);

    int unbind(ndk_handle handle);

projected:
    size_t max_size_;
#if defined(NDK_WIN32)
    std::map<ndk_hanlde, reactor_handle_tuple<REACTOR> *>::iterator reactor_handle_map_iter;
    std::map<ndk_hanlde, reactor_handle_tuple<REACTOR> *> reactor_handle_map_t;
    reactor_handle_map_t reactor_handle_map_;
#else
    //
    reactor_handle_tuple<REACTOR> *reactor_handle_map_;
#endif
  };

  /**
   * @class multi_reactor
   * 
   * @brief The responsibility of this class is disgind to dispatch large 
   * numbers of fds use multiple Reactor, as one reactor that includes large 
   * numbers of fds and which alse process I/O operation with only one thread 
   * would impact concurrence. e.g. epoll_reactor
   */
  template<typename REACTOR>
    class multi_reactor : public reactor_impl
  {
  public:
    enum 
    {
      // The capacity of single reactor will increase along with 
      // the increasing concurrence service.
      reactor_num_fixed = 0x13,

      // The number of the reactors will increase along with the increasing
      // concurrence service.
      single_reactor_capacity_fixed = 0x14
    };
    multi_reactor()
    {
    }

    virtual ~multi_reactor()
    {
    }
    /**
     * If <strategy> equals reactor_num_fixed, then the argment <count>
     * is the number of reactors, else if <strategy> equals 
     * single_reactor_capacity_fixed , the argment <count> is the capacity of
     * single reactor serves. The <max_handles> is the number of 
     * the handles that all Reactors can poll, if 0 , then the value is the number
     * of a process can allocate(e.g. result of `ulimit -n').
     */
    int init(int strategy = reactor_num_fixed, 
             int count = 4,
             int max_handles = 0);

    //
    virtual int close(void);

    // Not implement
    virtual int notify(event_handler *eh = 0,
                       reactor_mask mask = event_handler::except_mask,
                       const time_value *timeout = 0);

    // Not implement
    virtual int handle_events(const time_value *max_wait_time = 0);

    // Not implement
    virtual int deactivated(void);

    // Not implement
    virtual void deactivate(int do_stop);

    // ++ Register and remove Handlers.
    /**
     * Register <event_handler> to certain reactor with <mask>. 
     * The I/O handle will always come from <handle> on the <event_handler>.
     */
    virtual int register_handler(event_handler *event_handler,
                                 reactor_mask mask);

    /**
     * <handle> will be register to certain reactor.
     * Others is same to <Reactor>
     */
    virtual int register_handler(ndk_handle handle, 
                                 event_handler *event_handler,
                                 reactor_mask mask);

    /**
     * Removes <event_handler>.  Note that the I/O handle will be
     * obtained using <handle> method of <event_handler> .  If
     * <mask> == <event_handler::dont_call> then the <handle_close>
     * method of the <event_handler> is not invoked.
     */
    virtual int remove_handler(event_handler *event_handler,
                               reactor_mask mask);

    /**
     * Removes <handle>.  If <mask> == <event_handler::dont_call>
     * then the <handle_close> method of the associated <event_handler>
     * is not invoked.
     */
    virtual int remove_handler(ndk_handle handle,
                               reactor_mask mask);

    // ++ Timer management.

    /**
     * Schedule a timer event.
     * Schedule a timer event that will expire after an <delay> amount
     * of time.  The return value of this method, a timer_id value,
     * uniquely identifies the <event_handler> in the Reactor's internal
     * list of timers.  This timer_id value can be used to cancel the 
     * timer with the cancel_timer() call. 
     */
    virtual int schedule_timer(event_handler *event_handler,
                               const void *arg,
                               const time_value &delay,
                               const time_value &interval = time_value::zero);

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
     * avoid memory leaks, also pass the argument <event_handler::timer_mask>. 
     */
    virtual int cancel_timer(int timer_id,
                             const void **arg = 0,
                             int dont_call_handle_close = 1);

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
    virtual int cancel_timer(event_handler *event_handler,
                             int dont_call_handle_close = 1);

    /**
     * <count> is the number of reactors in MultiReactor, 
     * on -1 will stop all reactors.
     */
    void stop_reactors(int count = -1);

    /**
     * Returns the current size of the Reactor's internal descriptor
     * table.
     */
    virtual size_t size(void) const;

    // Not implement this method.
    virtual size_t curr_payload();

    // + Only the owner thread can perform a <handle_events>.

    // Set the new owner of the thread and return the old owner.
    virtual void owner(thread_t thr_id);

    // Return the current owner of the thread.
    virtual thread_t owner(void);
  protected:
    // Append a new REACTOR , return 0 if create success, other else
    // return -1.
    int append_reactor(void);

    // Inner method.
    virtual int register_handler_i(ndk_handle handle, 
                                   event_handler *event_handler,
                                   reactor_mask mask);

    // Inner method.
    virtual int remove_handler_i(ndk_handle handle, 
                                 reactor_mask mask);
  protected:
    // 
    int	strategy_;

    //  
    int	reactor_count_;

    //
    int max_reactor_count_;

    //
    int max_handles_;

    // 
    int reactor_index_;

    //
    int single_reactor_capacity_;

    //
    thread_mutex mutex_;

    REACTOR **reactors_;

    multi_reactor_handle_repository<REACTOR> *reactors_repository_;
  };
} // namespace ndk

#endif // NDK_MULTI_REACTOR_H_

