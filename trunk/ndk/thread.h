// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 02:24
 */
//========================================================================

#ifndef NDK_THREAD_H_
#define NDK_THREAD_H_

#include "ndk/types.h"

/**
 * @class thread

 * @brief 
 */
namespace ndk
{
  class thread
  {
  public:
    enum
    {	
      thr_join   = 1L << 1 ,
      thr_detach = 1L << 2
    };
    // return threadid 
    static thread_t self();

    // 
    static int thr_equal(thread_t thr_id_1, thread_t thr_id_2);

    // send a signal to the thread.
    static int kill(thread_t, int signum);

    // yield the thread to another. drop the cpu time
    static void yield(void);

    /**
     * Cancel a thread.
     * This method is only portable on platforms, such as POSIX pthreads,
     * that support thread cancellation.
     */
    static int cancel(hthread_t thr_id);

    // test the cancel
    static void testcancel(void);

    // create a thread
    // return : 0, success , -1 failed
    static int spawn(thread_func_t func, 
                     void *arg  = 0,
                     thread_t *thr_id = 0,
                     int flags = thr_join,
                     hthread_t *thr_handle = 0,
                     size_t stack_size = 0
                    );

    /** create n thread
     * return : >0 the number of create successfully
     *           0 create failed
     */
    static int spawn_n(size_t thr_num,
                       thread_func_t func, 
                       void *arg  = 0,
                       thread_t thr_id[] = 0,    // 
                       int flags = thr_join,
                       hthread_t thr_handles[] = 0,    // 
                       size_t stack_size = 0   // shared
                      );

    /**
     * retval  0 for success
     * retval  -1(with errno set) for failure.
     */
    static int join(hthread_t thr_id);

    // 
    static int detach(hthread_t thr_id);
  };
} // namespace ndk

#endif // NDK_THREAD_H_

