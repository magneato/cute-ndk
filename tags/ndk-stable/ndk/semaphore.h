// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 02:24
 */
//========================================================================

#ifndef NDK_SEMAPHORE_H_
#define NDK_SEMAPHORE_H_

#include "ndk/ndk.h"
#include "ndk/thread.h"
#include "ndk/time_value.h"

#include <semaphore.h>

namespace ndk
{
  /**
   * @class semaphore
   *
   * @brief 
   */
  class semaphore
  {
  public:
    inline semaphore(const int init_num = 0)
    {
      ::sem_init(&sem_, 0, init_num);
    }

    inline ~semaphore()
    {
      int retry = 0;
      while ((::sem_destroy(&sem_) == -1) && retry < 3)
      {
        if (errno == EBUSY)
        {
          ++retry;
          this->post();
          thread::yield();
        }else
          break;
      }
    }

    inline int wait()
    {
      int ret = -1;
      do{
        ret = ::sem_wait(&sem_);
      }while (ret == -1 && errno == EINTR);
      return ret;
    }

    inline int wait(const time_value *time_out)
    {
      if (time_out == 0) return this->wait();
      struct timespec ts;
      ts = time_value::gettimeofday() + *time_out;
      int ret = -1;
      do{
        ret = ::sem_timedwait(&this->sem_, &ts);
      }while (ret == -1 && errno == EINTR);
      return ret;
    }

    inline int trywait()
    {
      return ::sem_trywait(&this->sem_);
    }

    inline int post()
    {
      return ::sem_post(&this->sem_);
    }

    // return the number of post successfully.
    inline int post(int count)
    {
      int ret = 0;
      while (count-- > 0)
      {
        ret += ::sem_post(&sem_) == 0 ? 1 : 0;
      }
      return ret;
    }
  private:
    sem_t sem_;
  };
} // namespace ndk

#endif // NDK_SEMAPHORE_H_

