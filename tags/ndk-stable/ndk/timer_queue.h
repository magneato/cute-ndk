// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-06 15:25
 */
//========================================================================

#ifndef NDK_TIMER_QUEUE_H_
#define NDK_TIMER_QUEUE_H_

#include "ndk/ndk.h"
#include "ndk/time_value.h"
#include "ndk/event_handler.h"

namespace ndk
{
  /**
   * @class timer_node
   *
   * @brief 
   */
  class timer_node
  {
  public:
    timer_node();
    // 
    ~timer_node();

    void release_cron_entry();

    char **alloc_cron_entry();

    void set(int timer_id,
             const event_handler *handler,
             const void *arg,
             const time_value &t,
             const time_value &i,
             timer_node *p = 0,
             timer_node *n = 0);

    //
    timer_node & operator = (const timer_node &tn);

    // Get the timer handler.
    inline event_handler *handler() { return this->handler_; }

    // Set the timer handler
    void handler(event_handler *h)  { this->handler_ = h;    }

    // Get the handler arg.
    const void *arg(void)           { return this->arg_;     }

    // Set the handler arg
    void arg(void *arg)             { this->arg_ = arg;      }

    // Get the crontab entry
    char **crontab_entry(void)      { return this->crontab_entry_; }

    // Set the crontab entry
    void crontab_entry(char **en)   { this->crontab_entry_ = en; }
    
    // Get the timer value.
    const time_value &timer_value(void) const
    {
      return this->timer_value_;
    }

    // Set the timer value.
    void timer_value(const time_value &tv)
    {
      this->timer_value_ = tv;
    }

    // Get the timer interval.
    const time_value &interval(void) const
    {
      return this->interval_;
    }

    // Set the timer interval.
    void interval(const time_value &interval)
    {
      this->interval_ = interval;
    }

    // Get the previous pointer.
    timer_node *prev(void)          { return this->prev_;    }

    // Set the previous pointer.
    void prev(timer_node *prev)     { this->prev_ = prev;    }

    // Get the next pointer.
    timer_node *next(void)          { return this->next_;    }

    // Set the next pointer.
    void next(timer_node *next)     { this->next_ = next;    }

    // Get the timer_id.
    int timer_id(void) const        { return this->timer_id_;}

    // Set the timer_id.
    void timer_id(int id)           { this->timer_id_ = id;  }

    // Set timer type
    void type(const int t)          { this->type_ = t;       }

    // Get timer type
    int type(void)                  { return this->type_;    }
  private:
    // Id of this timer(used to cancel timers before they expire).
    int timer_id_;

    int type_;

    //
    char **crontab_entry_;

    // Timer handler object
    event_handler *handler_;

    // If timer expires then <arg> is passed in as the parameter of 
    // the handler
    const void *arg_;

    // Time until the timer expires.
    time_value timer_value_;

    // If this is a periodic timer this holds the time until the next
    // timeout.
    time_value interval_;

    // Pointer to previous timer.
    timer_node *prev_;

    // Pointer to next timer.
    timer_node *next_;
  };
  /**
   * @class timer_queue
   * 
   * @brief 
   */
  class timer_queue : public event_handler
  {
  public:
    timer_queue();

    timer_queue(int max_size, int pre_alloc_num = 1024);

    virtual ~timer_queue();

    // initialize members.
    void init(int pre_alloc_num);

    // true if queue is empty, else false.
    inline virtual bool is_empty(void) const
    {
      return this->curr_size_ == 0;
    }

    // returns the time of the earlier node in the timer_queue.
    inline const time_value &earliest_time(void) const
    {
      return this->timer_heap_[0]->timer_value();
    }

    // for crontab timers
    virtual int handle_timeout(const void * /*arg*/, 
                               const time_value & /*curent_time*/);

    /**
     * Schedule a timer that will expire after an <delay> amount
     * of time.  The return value of this method, a timer_id value,
     * uniquely identifies the <eh> in the Reactor's internal
     * list of timers.  This timer_id value can be used to cancel the 
     * timer with the cancel() call. 
     */
    inline virtual int schedule(const event_handler *eh,
                                const void *arg,
                                const time_value &delay_time,	
                                const time_value &interval_time)
    {
      return this->schedule_i(eh,
                              arg, 
                              time_value::gettimeofday() + delay_time,
                              interval_time);
    }
    // Format:
    // *       *       *       *       *
    // Min    Hour    Mday    Month   Wday

    // The fields are:
    // Min       minute of execution, 0-59
    // Hour      hour of execution, 0-23
    // Mday      day of month of execution, 1-31
    // Month     month of execution, 1-12 (or names)
    // Wday      day of week of execution, 0-7 (1 = monday, 0 or 7 = sunday, 
    //           or names)

    // Possible values:
    // *       matches all values, e.g. a * in month means: "every month"
    // x-y     matches the range x to y, e.g. 2-4 in Mday means "on the 
    //         2nd, 3rd, and 4th of the month"
    // x/n     in range x with frequency n, e.g. */2 in Hour means "every other hour"

    // Months can be names: jan, Feb, mAr (case insensitive). (use 3 chars)
    // Weekdays can be names, e.g. sun, Mon, tUe. (use 3 chars, no case)

    // Notes:
    // 1). Ranges and lists can be mixed. 
    //     e.g. 1,3-5 (means "1 3 4 5") 1,3-7/2 (means "1 3 5 7")
    // 2). Ranges can specify 'step' values. '10-16/2' is like '10,12,14,16'
    // 3). Ranges can be opened region. e.g. '2-' in hour means '2-23',
    //     '-15' in hour means '0-15', '-' is '*'
    // 4). List support '1,2,3,5-9,15-21/2,25/2,50-8,*/2,*,,' (this can be one entry,
    //     means '*', will calculate the collection of region.)

    // Special entries:
    // ------          -------
    // @yearly         Run once a year, "0 0 1 1 *".
    // @monthly        Run once a month, "0 0 1 * *".
    // @weekly         Run once a week, "0 0 * * 0".
    // @daily          Run once a day, "0 0 * * *".
    // @hourly         Run once an hour, "0 * * * *".

    // Examples:
    // '30 0 1 1,6,12 *'       00:30 Hrs on 1st of Jan, June & Dec.
    // '* * 1,15 * Sun'        Will run on the first and fifteenth AND every Sunday;
    // '* 20 * 10 1-5'         8.00 PM every weekday (Mon-Fri) only in Oct.
    // '0 7-23/3 * * *'        Every 3-hours while awake
    // '*/5 6-13 * * mon-fri'  Every 5 minutes during market hours
    // '0 12 * * 0,2,4'        Will run at noon every sun,tues,thurs 
    // '0 18-7 * * 1-5'        6pm-7am every hour during weekdays.
    int crontab(const event_handler *eh, const void *arg, const char *entry);

    /**
     * Resets the crontab-entry of the timer represented by <timer_id> to
     * <entry>.  Returns 0 if successful, -1 if not.
     */
    virtual int reset_crontab(int timer_id, const char *entry);

    /**
     * Resets the interval of the timer represented by <timer_id> to
     * <interval>. If <interval> is equal to <0> the timer will 
     * become a non-rescheduling timer.  Returns 0 if successful, 
     * -1 if not.
     */
    virtual int reset_interval(int timer_id,
                               const time_value &interval_time);

    /**
     * Cancel all timer. If <dont_call_handle_close> is 0 then the 
     * <callback> will be invoked, which typically invokes the 
     * <handle_close> hook.  Returns number of timers cancelled.
     *
     * Note: The method will canll event_handler::handle_close() when
     * <dont_call_handle_close> is zero, but the first argument which 
     * points to the <arg> is zero, because this <eh> maybe register 
     * multiple argument and we don't know which should be passed out.
     */
    virtual int cancel(const event_handler *eh,
                       int dont_call_handle_close = 1);

    /**
     * Cancel the single timer that matches the <timer_id> value(which
     * was returned from the <schedule> method).  If <arg> is non-NULL
     * then it will be set to point to the <argument> passed in when the 
     * timer was registered. This makes it possible to free up the memory 
     * and avoid memory leaks. If <dont_call> is 0 then the <handle_close> 
     * will be invoked. event_handler::handle_close() will pass the argument
     * that passed in when the handler was registered, This makes it possible 
     * to release the memory of the <arg> in handle_close.
     *
     * Returns 0 if cancellation succeeded and -1 if the <timer_id> 
     * wasn't found.
     */
    virtual int cancel(int timer_id,
                       const void **arg = 0,
                       int dont_call_handle_close = 1);

    // Determine the next event to timeout. 
    virtual time_value* calculate_timeout(const time_value* max_wait_time);

    // Determine the next event to timeout. 
    virtual time_value* calculate_timeout(const time_value* max_wait_time,
                                          time_value* the_timeout);

    /** 
     * Run the <functor> for all timers whose values are <= <cur_time>.
     * Returns the number of timers canceled.
     */
    virtual int expire(const time_value &current_time);

  protected:
    // Return nonzero on dispatch an timer
    int dispatch_timer(const time_value &current_time,
                       timer_node &dispatched_node);

    int upcall(event_handler *event_handler,
               int(event_handler::*callback)(const void *act, const time_value &),
               const void *arg,
               const time_value &current_time);

    // Schedule a timer.
    int schedule_i(const event_handler *eh,
                   const void *arg,
                   const time_value &delay_time,
                   const time_value &interval_time);
    //
    int parse_cron_entry(const char *entry, char **item);

    //
    int parse_cron_entry_i(const char *entry, char *item,
                           int first, int last, int count);

    // pops and returns a new timer id from the freelist.
    int alloc_timerid();

    // release <old_id> onto the freelist.
    void release_timerid(int old_id);

    // Reschedule an "interval" ACE_Timer_Node.
    void reschedule(timer_node *);    

    // Factory method that allocates a new node.
    timer_node *alloc_node(int type);

    // Factory method that frees a previously allocated node.
    void free_node(timer_node *); 

    // Insert a new_node into the heap and restore the heap property.
    void insert(timer_node *new_node);

    // Remove and return the a slotth timer_node and restore the
    // heap property.
    timer_node *remove(int slot);

    /**
     * Doubles the size of the heap and the corresponding timer_ids array.
     * If preallocation is used, will also double the size of the
     * preallocated array of timer_node.
     */
    void grow_heap(void);

    // Restore the heap property, starting at @a slot.
    void reheap_up(timer_node *new_node,
                   int slot,
                   int parent);

    // Restore the heap property, starting at @a slot.
    void reheap_down(timer_node *moved_node,
                     int slot,
                     int child);

    // Copy <moved_node> into the @a slot slot of <heap_> and move
    // a slot into the corresponding slot in the <timer_id_> array.
    void copy(int slot, timer_node *moved_node);

    /**
     * Removes the earliest node from the queue and returns it. Note that
     * the timer is removed from the heap, but is not freed, and its ID
     * is not reclaimed. The caller is responsible for calling either
     * reschedule() or free_node() after this function returns. Thus,
     * this function is for support of timer_queue::expire and should 
     * not be used unadvisedly in other conditions.
     */
    timer_node *remove_first(void);

    // for test.
    void verify_heap();
  private:
    // Maximum size of the heap 
    int max_size_;

    int curr_size_;

    int timer_heap_size_;

    timer_node **timer_heap_;

    timer_node *free_timer_nodes_;

    timer_node *free_crontab_timers_;

    timer_node *crontab_timers_;

    int *timer_ids_;

    int timer_ids_size_;

    int min_timer_id_;

    // Returned by <calculate_timeout>.
    time_value timeout_;
  private:
    timer_queue(const timer_queue &);
    void operator = (const timer_queue &); 
  };
} // namespace ndk

#endif // NDK_TIMER_QUEUE_H_

