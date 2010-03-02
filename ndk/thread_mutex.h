// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 02:24
 */
//========================================================================

#ifndef NDK_THREAD_MUTEX_H_
#define NDK_THREAD_MUTEX_H_

#include "time_value.h"

#include <pthread.h>
#include <sys/time.h>

namespace ndk
{
  /**
   * @class thread_mutex
   *
   * @brief 
   */
  class thread_mutex
  {
  public:
    inline thread_mutex()
    {
      ::pthread_mutex_init(&this->mutex_, NULL);
    }

    inline ~thread_mutex()
    {
      ::pthread_mutex_destroy(&this->mutex_);
    }

    inline int acquire(void)
    {
      return ::pthread_mutex_lock(&this->mutex_) == 0 ? 0 : -1;
      return 0;
    }

    // acquire lock ownership in <timeout> period 
    int acquire(const time_value *timeout) 
    {
      struct timespec ts;
      ts = time_value::gettimeofday() + *timeout;
      return ::pthread_mutex_timedlock(&this->mutex_, &ts) == 0 ? 0 : -1;
    }

    // not block if someone already had the lock , errno is EBUSY
    int try_acquire(void) 
    {
      return ::pthread_mutex_trylock(&this->mutex_) == 0 ? 0 : -1;
    }

    // release lock ownership 
    int release(void) 
    {
      return ::pthread_mutex_unlock(&this->mutex_) == 0 ? 0 : -1;
      return 0;
    }

    // return mutex_t
    const pthread_mutex_t &lock() const 
      {
        return this->mutex_;
      }
  private:
    pthread_mutex_t mutex_;
  };
} // namespace ndk

#endif // NDK_THREAD_MUTEX_H_

