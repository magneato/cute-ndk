#include "ndk/thread.h"

#include "ndk/global_macros.h"

#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>
#include <limits.h>

namespace ndk
{
thread_t thread::self()
{
  return ::pthread_self();
}
int thread::thr_equal(thread_t thr_id_1, thread_t thr_id_2)
{
  return ::pthread_equal(thr_id_1, thr_id_2);
}
int thread::kill(thread_t thr_id, int signum)
{
  int result = ::pthread_kill(thr_id, signum);
  if (result != 0)
  { errno = result; return -1; }
  return 0;
}
void thread::yield(void)
{
  ::sched_yield();
}
int thread::cancel(hthread_t thr_handle)
{
  return ::pthread_cancel(thr_handle) == 0 ? 0 : -1;
}
void thread::testcancel(void)
{
  ::pthread_testcancel();
}
int thread::spawn_n(size_t thr_num, 
                    thread_func_t func, 
                    void *arg/* = 0*/, 
                    thread_t *thr_ids/* = 0*/, 
                    int flags/* = thr_join*/, 
                    hthread_t *thr_handles/* = 0*/, 
                    size_t stack_size/* = 0*/)
{
  size_t i = 0;
  for (; i < thr_num; ++i)
  {
    thread_t thr_id = 0;
    hthread_t thr_handle = 0;
    if (thread::spawn(func, 
                      arg, 
                      &thr_id, 
                      flags, 
                      &thr_handle, 
                      stack_size) == 0)
    {
      if (thr_ids != 0 && thr_id != NULL_thread)
        thr_ids[i] = thr_id;
      if (thr_handles != 0 && thr_handle != 0)
        thr_handles[i] = thr_handle;
    }else
      break;
  }
  return i;
}
int thread::join(hthread_t thr_handle)
{
  int result = ::pthread_join(thr_handle, NULL);
  if (result != 0)
  {
    errno = result;
    return -1;
  }
  return 0;
}
int thread::detach(thread_t thr_id)
{
  int result = ::pthread_detach(thr_id);
  if (result != 0)
  {
    errno = result;
    return -1;
  }
  return 0;
}
int thread::spawn(thread_func_t func, 
                  void *arg/* = NULL*/, 
                  thread_t *thr_id/* = 0*/, 
                  int flags/* = thr_join*/, 
                  hthread_t *thr_handle/* = 0*/, 
                  size_t stack_size/* = 0*/)
{
  int ret_val = 0;
  // default thread stack size is 8M
  pthread_attr_t attr;
  // init
  if (::pthread_attr_init(&attr) != 0) return -1;
  if (stack_size != 0)
  {
    if (stack_size < PTHREAD_STACK_MIN)
      stack_size = PTHREAD_STACK_MIN; 

    if (::pthread_attr_setstacksize(&attr, stack_size) != 0)
    {
      ::pthread_attr_destroy(&attr);
      return -1;
    }
  }
  thread_t id = NULL_thread;
  if (thr_id == 0)
    thr_id = &id;
  if (::pthread_create(thr_id, &attr, func, arg) == 0)
  {
    if (NDK_BIT_ENABLED(flags, thr_detach))
      ::pthread_detach(*thr_id);
  }else
  {
    ret_val = -1;
  }
  if (thr_handle)
    *thr_handle = *thr_id;
  // release
  ::pthread_attr_destroy(&attr);
  return ret_val;
}
} // namespace ndk
