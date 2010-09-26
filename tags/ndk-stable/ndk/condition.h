// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 02:24
 */
//========================================================================

#ifndef NDK_CONDITION_H_
#define NDK_CONDITION_H_

#include "ndk/ndk.h"
#include "ndk/time_value.h"

#include "ndk/thread.h"    // for thread::yield

namespace ndk
{
  /**
   * @class condition
   *
   * @brief 
   */
  template<typename mtx_t>
  class condition
  {
  public:
    inline condition(mtx_t &m)
      : mutex_(m)
    {
      ::pthread_cond_init(&this->cond_, NULL);
    }

    inline ~condition()
    {
      int retry = 0;
      while ((::pthread_cond_destroy(&this->cond_) == EBUSY)
             && retry++ < 3)
      {
        this->broadcast();
        ndk::thread::yield();
      }
    }

    inline int wait(void)
    {
      int result = ::pthread_cond_wait(&this->cond_,
                                       const_cast<pthread_mutex_t*>(&this->mutex_.lock()));
      if (result != 0)	{ errno = result; result = -1;}
      return result;
    }

    inline int wait(const time_value *time_out)
    {
      if (time_out == 0)
        return this->wait();

      struct timespec ts;
      ts = time_value::gettimeofday() + *time_out;
      // shall not return an error code of [EINTR]
      int result = ::pthread_cond_timedwait(&this->cond_,
                                            const_cast<pthread_mutex_t*>(&this->mutex_.lock()),
                                            &ts);
      if (result != 0) { errno = result; result = -1; }
      return result;
    }

    inline int signal(void)
    {
      return ::pthread_cond_signal(&this->cond_) == 0 ? 0 : -1;
    }

    inline int broadcast(void)
    {
      return ::pthread_cond_broadcast(&this->cond_) == 0 ? 0 : -1;
    }

    inline mtx_t &mutex(void)
    {
      return this->mutex_;
    }
  private:
    mtx_t &mutex_;
    pthread_cond_t cond_;
  };
} // namespace ndk

#endif // NDK_CONDITION_H_

