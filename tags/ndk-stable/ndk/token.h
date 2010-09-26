// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-02 22:52
 */
//========================================================================

#ifndef NDK_TOKEN_H_
#define NDK_TOKEN_H_

#include "ndk/types.h"
#include "ndk/guard.h"
#include "ndk/condition.h"
#include "ndk/thread_mutex.h"

namespace ndk
{
  // Forward declarations
  //
  class time_value;

  /**
   * @class token
   * 
   * @brief 
   */
  class token
  {
  public:
    enum
    {
      FIFO    = 1,
      LIFO	  = 2
    };
    token(int q_strategy = FIFO)
    : waiters_(0),
    in_use_(0),
    nesting_level_(0),
    queueing_strategy_(q_strategy),
    owner_(NULL_thread),
    lock_()
    { }

    virtual ~token() { }

    // == Guard method
    /**
     * Acquire the lock, sleeping until it is obtained or until the
     * expiration of >timeout>, If some other thread currently holds 
     * the token then <leep_hook> is called before our thread goes 
     * to sleep. This <sleep_hook> can be used by the requesting 
     * thread to unblock a token-holder that is sleeping, e.g., by 
     * means of writing to a pipe(the Reactor uses this functionality).
     * Return values: 0 hold -1 if failure or timeout occurs
     */
    inline virtual int acquire(void(*sleep_hook_func)(void *),
                               void *arg = 0,
                               const time_value *timeout = 0)
    {
      return this->shared_acquire(sleep_hook_func, arg, timeout, write_token);
    }

    /**
     * This behaves just like the previous <acquire> method, except that
     *  it invokes the virtual function called <sleep_hook> that can be
     *  overridden by a subclass of token.
     */
    inline virtual int acquire(const time_value *timeout = 0)
    {
      return this->shared_acquire(0, 0, timeout, write_token);
    }

    // Behaves like acquire() but at a lower priority.
    inline virtual int acquire_read(void(*sleep_hook)(void *),
                                    void *arg = 0,
                                    const time_value *timeout = 0)
    {
      return this->shared_acquire(sleep_hook, arg, timeout, read_token);
    }
    /**
     * This should be overridden by a subclass to define the appropriate
     * behavior before <acquire> goes to sleep.  By default, this is a
     * no-op...
     */
    virtual void sleep_hook(void) { }

    // Implements a non-blocking <acquire>
    inline virtual int try_acquire(void)
    {
      return this->shared_acquire(0, 0, &time_value::zero, write_token);
    }

    // Relinquish the lock.  If there are any waiters then the next one
    // in line gets it.
    inline virtual int release(void)
    {
      guard<thread_mutex> g(this->lock_);
      if (this->nesting_level_ > 0) --this->nesting_level_;
      else this->wakeup_next_waiter();
      return 0;
    }

    // Return the number of threads that are currently waiting to get
    // the token
    inline int waiters(void)
    {
      guard<thread_mutex> g(this->lock_);
      return this->waiters_;
    }

    // Return the id of the current thread that owns the token.
    thread_t current_owner(void)
    {
      guard<thread_mutex> g(this->lock_);
      return this->owner_;
    }
  protected:
    // The following structure implements a LIFO/FIFO queue of waiter threads
    // that are asleep waiting to obtain the token.
    class token_queue_entry
    {
    public:
      token_queue_entry(thread_mutex &lock,
                        thread_t thr_id)
        : runable_(0),
        next_(0),
        thread_id_(thr_id),
        cond_(lock)
      { }

      // Entry blocks on the token.
      int wait(const time_value *timeout) { return this->cond_.wait(timeout); }

      // Notify(unblock) the entry.
      int signal(void)                    { return this->cond_.signal();      }

      // Ok to run.
      int runable_;

      // Pointer to next waiter.	
      token_queue_entry *next_;

      // Thread id of this waiter.
      thread_t thread_id_;

      // Condition object used to wake up waiter when it can run again.
      condition<thread_mutex> cond_;
    };
    enum token_opt_type
    {
      read_token    = 10,
      write_token
    };
    class token_queue
    {
    public:
      token_queue()
        : head_(0),
        tail_ (0)
      { }

      // Remove a waiter from the queue.
      void remove_entry(token_queue_entry *);

      // Insert a waiter into the queue.
      void insert_entry(token_queue_entry &entry,
                        int queue_strategy = FIFO);

      // Head of the list of waiting threads.
      token_queue_entry *head_;

      // Tail of the list of waiting threads.
      token_queue_entry *tail_;
    };
  private:
    // Implements the <acquire> and <tryacquire> methods above.
    int shared_acquire(void(*sleep_hook_func)(void *),
                       void *arg,
                       const time_value *timeout,
                       token_opt_type opt_type);

    // Wake next in line for ownership.
    void wakeup_next_waiter(void);
  private:
    // Number of waiters
    int waiters_;

    // Some thread(i.e., <owner_>) is useing the lock, we need this
    // extra variable to deal with POIX pthreads madness
    int in_use_; 

    // Cureent nest level
    int nesting_level_;

    // Queueing strategy, LIFO/FIFO.
    int queueing_strategy_;

    // Current owner of the lock
    thread_t owner_;

    // guard
    thread_mutex lock_;

    // A queue of writer threads.
    token_queue writers_;

    // A queue of reader threads.
    token_queue readers_;
  };
} // namespace ndk

#endif // NDK_TOKEN_H_

