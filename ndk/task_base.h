// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-21 16:26
 */
//========================================================================

#ifndef NDK_TASK_BASE_H_
#define NDK_TASK_BASE_H_

#include "ndk/guard.h"
#include "ndk/thread.h"
#include "ndk/unused_arg.h"
#include "ndk/thread_mutex.h"
#include "ndk/event_handler.h"

namespace ndk
{
  /**
   * @class task_base
   * 
   * @brief
   */
  class task_base : public event_handler
  {
  public:
    task_base()
      : thr_count_(0)
    { }

    virtual ~task_base()
    { }

    //
    virtual int open(void *args = 0)
    { 
      unused_args(args);
      return -1; 
    }

    /**
     * Hook called when during thread exit normally,
     * In general, this method shouldn't be called directly 
     * by an application, particularly if the <task> is 
     * running as an Active Object.Instead, a special message 
     * should be passed into the <task> via the <put> method 
     * defined below, and the <svc> method should interpret 
     * this as a flag to shutdown the <task>.
     */
    virtual int close()
    { return -1; }
    
    // Run by a daemon thread to handle deferred processing.
    virtual int svc() = 0;

    /**
     * return 0  success
     * return -1 failed
     */
    virtual int activate(int flags = thread::thr_join,
                         size_t n_threads = 1,
                         thread_t thread_ids[] = 0,
                         hthread_t thread_handles[] = 0,
                         int append_thr = 0,
                         size_t stack_size = 0)
    {
      guard<thread_mutex> g(this->lock_); // avoid mutithread reentry
      if (this->thr_count_ > 0 && append_thr == 0)
        return 1;

      //

      if (thread::spawn_n(n_threads,
                          &task_base::svc_run,
                          (void*)this,
                          thread_ids,
                          flags,
                          thread_handles,
                          stack_size) == -1)
        return -1;
      this->thr_count_ += n_threads;
      return 0;
    }

    // Returns the number of threads currently running within a task.
    size_t thr_count(void) const
    { 
      guard<thread_mutex> g(this->lock_);
      return this->thr_count_; 
    }

    // retval 0  Success.
    // retval -1 Failure
    virtual int wait(thread_t thread_ids[], size_t size)
    {
      for (size_t i = 0; i < size; ++i)
        thread::join(thread_ids[i]);
      return 0;
    }
private:
    // Routine that runs the service routine as a daemon thread.
    static thread_func_return_t svc_run(void *args)
    {
      if (args == 0) return 0;
      task_base* t = (task_base *)args;
      int svc_status = t->svc();
      thread_func_return_t exit_status;
      exit_status = reinterpret_cast<thread_func_return_t> (svc_status);

      t->cleanup();
      return exit_status;   // maybe used for pthread_join
    }

    // clean up
    void cleanup(void)
    {
      guard<thread_mutex> g(this->lock_);
      this->thr_count_--;
      this->close();
    }

    // Disallow these operations.
    task_base & operator = (const task_base &);
    task_base(const task_base &);
protected:
    // Count of the number of threads running within the task. 
    size_t thr_count_;

    thread_mutex lock_;
  };
} // namespace ndk

#endif // NDK_TASK_BASE_H_

