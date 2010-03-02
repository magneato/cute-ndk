// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-07-12 13:49
 */
//========================================================================

#ifndef NDK_RW_THREAD_MUTEX_H_
#define NDK_RW_THREAD_MUTEX_H_

#include "ndk/ndk.h"

#include <pthread.h>

namespace ndk
{
  /**
   * @class rw_thread_mutex
   * 
   * @brief
   */
  class rw_thread_mutex
  {
  public:
    rw_thread_mutex()
    { pthread_rwlock_init(&this->rwlock_, NULL); }

    ~rw_thread_mutex()
    { pthread_rwlock_destroy(&this->rwlock_); }

    //
    int acquire_read(void)
    {
      return ::pthread_rwlock_rdlock(&this->rwlock_) == 0 ? 0 : -1;
    }

    //
    int acquire_write(void)
    {
      return ::pthread_rwlock_wrlock(&this->rwlock_) == 0 ? 0 : -1;
    }

    //
    int try_acquire_read(void)
    {
      return ::pthread_rwlock_tryrdlock(&this->rwlock_) == 0 ? 0 : -1;
    }

    //
    int try_acquire_write(void)
    {
      return ::pthread_rwlock_trywrlock(&this->rwlock_) == 0 ? 0 : -1;
    }

    // unlock a readers/writer lock.
    int release(void)
    {
      return ::pthread_rwlock_unlock(&this->rwlock_) == 0 ? 0 : -1;
    }

  private:
    pthread_rwlock_t rwlock_;
  };
} // namespace ndk

#endif // NDK_RW_THREAD_MUTEX_H_

