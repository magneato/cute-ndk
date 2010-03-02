// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-21 16:53
 */
//========================================================================

#ifndef NDK_THREAD_MANAGER_H_
#define NDK_THREAD_MANAGER_H_

#include "ndk/thread.h"
#include "ndk/singleton.h"
#include "ndk/thread_mutex.h"

#include <map>
// The interface push_back of the std::deque if more fast than std::list
#include <deque>  

namespace ndk
{
  // Forward declarations.
  //
  class task_base;
  class thread_descriptor;

  /**
   * @class thread_manager
   * 
   * @brief
   */
  class thread_manager : public singleton<thread_manager>
  {
    friend class singleton<thread_manager>;
  public:
    // thread state 
    enum
    {
      // uninitialized
      thr_idle        = 1L << 1,

      // created but not yet running.
      thr_spawned     = 1L << 2,

      // thread is active(naturally, we don't know if it's actually
      // *running* because we aren't the scheduler...).
      thr_running     = 1L << 3,

      // thread is suspended.
      thr_suspended   = 1L << 4,

      // thread has been cancelled(which is an indiction that it needs 
      // to terminate...).
      thr_cancelled   = 1L << 5,

      // thread has shutdown, but the slot in the thread manager hasn't
      // been reclaimed yet.
      thr_terminated  = 1L << 6,

      // join operation has been invoked on the thread by thread manager.
      thr_joining     = 1L << 7
    };
  public:
    /** 
     * if <grp_id> is assigned, the newly spawned threads are 
     * added into the group. otherwise, the Thread_Manager assigns
     * an new unique group id
     * return : a unique group id that can be used to control
     * other threads added to the same group if success, -1 if failed
     */
    int spawn(thread_func_t func,
              void *arg  = 0,
              thread_t *thr_id = 0,
              int flags = thread::thr_join,
              hthread_t *thr_handle = 0,
              int grp_id = -1,
              size_t stack_size = 0,
              task_base *task = 0)
    {
      if (grp_id == -1) 
      {
        guard<thread_mutex> g(this->grp_id_mutex_);
        grp_id = this->grp_id_++;
      }
      if (this->spawn_i(func, arg, 
                        thr_id, 
                        flags, 
                        thr_handle,
                        grp_id, 
                        stack_size, 
                        task) == -1)
        return -1;
      return grp_id;
    }

    /**
     * spawn thr_num new threads, which execute <func> with 
     * argument <arg> if <grp_id> is assigned, the newly spawn
     * ed threads are added into the group. otherwise, the 
     * Thread_Manager assigns an new unique group id
     * return : >0, a unique group id that can be used to control 
     *          -1, one of the new thread create failed
     */
    int spawn_n(size_t thr_num,
                thread_func_t func,
                void *arg  = 0,
                thread_t thr_ids[] = 0,    //
                int flags = thread::thr_join,
                hthread_t thr_handles[] = 0,    //
                int grp_id = -1,
                size_t stack_size = 0,   // shared
                task_base *task = 0)
    {
      if (grp_id == -1) 
      {
        guard<thread_mutex> g(this->grp_id_mutex_);
        grp_id = this->grp_id_++;
      }
      //
      for (size_t n = 0; n < thr_num; ++n)
      {
        if (this->spawn_i(func, arg, 
                          thr_ids == 0 ? 0 : &thr_ids[n],
                          flags, 
                          thr_handles == 0 ? 0 : &thr_handles[n],
                          grp_id, 
                          stack_size, 
                          task) == -1)
          return -1;
      }
      return grp_id;
    }

    /**
     * return a count of the current number of threads active in 
     * the thread_manager
     */
    size_t thr_count(void);

    // get the group-id related with threadid
    int get_grp_id(thread_t thr_id);

    /**
     * join all thread that in the threadmanager 
     */
    int wait(void);

    // wait one task
    int wait_task(task_base *);

    // Set the state of the thread. Returns -1 if the thread is not
    // managed by this thread manager
    int set_thr_state(thread_t thr_id, size_t thr_state);

    // Get the state of the thread. Returns -1 if the thread is not
    // managed by this thread manager
    int get_thr_state(thread_t thr_id, size_t &thr_state);

    // ++ Cancel methods, which provides a cooperative thread-termination 
    // mechanism(will not block)..
    // Cancel a single thread.
    int cancel(hthread_t thr_handle, int async_cancel = 0);

    // Cancel a group of threads. 
    int cancel_grp(int grp_id, int async_cancel = 0);

    // True if <t_id> is cancelled, else false. Always return false if
    // <t_id> is not managed by the Thread_Manager.
    int testcancel(thread_t thr_id, int async_cancel = 0);

  protected:
    thread_manager()
      : grp_id_(0x1314)
    { }

    ~thread_manager()
    { }

    // append a thread in the table
    int append_thr(thread_t thr_id, 
                   hthread_t thr_handle,
                   int thr_state, 
                   int grp_id,
                   int flags,
                   task_base *task);
    //
    int spawn_i(thread_func_t func, 
                void *arg, 
                thread_t *thr_id, 
                int flags, 
                hthread_t *thr_handle,
                int grp_id, 
                size_t stack_size, 
                task_base *task)
    {
      thread_t id = NULL_thread;
      if (thr_id == 0) thr_id = &id;

      hthread_t handle = 0;
      if (thr_handle == 0) thr_handle = &handle;
      if (thread::spawn(func, arg, thr_id, flags, thr_handle, stack_size) == -1)
        return -1;
      return this->append_thr(*thr_id, 
                              *thr_handle, 
                              thread_manager::thr_spawned, 
                              grp_id, 
                              flags, 
                              task);
    }

    // Not bind lock
    thread_descriptor *find_thread_i(thread_t thr_id);
    thread_descriptor *find_thread_by_handle(hthread_t thr_handle);
    thread_descriptor *find_thread_i(thread_t thr_id, int grp_id);

    //
    void remove_thr_i(thread_t thr_id);
    void remove_thr_i(thread_t thr_id, int grp_id);
    //
  private:
    // keep a list of thread descriptors within thread manager
    typedef std::deque<thread_descriptor*> thr_list_t;
    typedef thr_list_t::iterator         thr_list_itor;
    typedef std::map<int/*grp_id*/, thr_list_t> thr_table_t;
    typedef thr_table_t::iterator        thr_table_itor;

    // keep track of the next group id to assign
    int	   grp_id_;
    thread_mutex grp_id_mutex_;

    thr_table_t		   thr_table_;
    thread_mutex	   thr_table_mutex_;
  protected:
    void update_thr_term_list_copy(thr_list_t &term_thr_list_copy, int &thr_count);
  };
  class thread_descriptor
  {
    friend class thread_manager;
  public:
    thread_descriptor()
      : grp_id_(-1),
      thr_state_(thread_manager::thr_idle),
      thr_flags_(0),
      thr_id_(NULL_thread),
      thr_handle_(0),
      task_(0),
      thr_mgr_(0)
    { }
    //
    ~thread_descriptor()
    { }
    // 
    thread_manager* thr_mgr(void);
    //
    void thr_mgr(thread_manager *thr_mgr);
    //
  protected:
    // 
    int		      grp_id_;
    int		      thr_state_;
    int		      thr_flags_;
    thread_t	      thr_id_;
    hthread_t	      thr_handle_;
    task_base*	    task_;
    thread_manager* thr_mgr_;
  };
} // namespace ndk

#endif // NDK_THREAD_MANAGER_H_

