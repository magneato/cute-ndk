// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-07-12 13:49
 */
//========================================================================

#ifndef NDK_RECURSIVE_THREAD_MUTEX_H_
#define NDK_RECURSIVE_THREAD_MUTEX_H_

#include "ndk/thread.h"

namespace ndk
{
  /**
   * @class recursive_thread_mutex
   * 
   * @brief
   */
  class recursive_thread_mutex
  {
  public:
    recursive_thread_mutex()
      : nesting_level_(0),
      owner_thr_(0)
    { ::pthread_mutex_init(&this->mutex_, 0); }

    ~recursive_thread_mutex()
    { ::pthread_mutex_destroy(&this->mutex_); }

    //
    int acquire(void)
    {
      thread_t t_id = thread::self();
      if (thread::thr_equal(this->owner_thr_, t_id))
        ++this->nesting_level_;
      else
      {
        int ret = 0;
        if ((ret = ::pthread_mutex_lock(&this->mutex_)) == 0)
        {
          this->owner_thr_ = t_id;
          this->nesting_level_ = 0;
        }else
        {
          errno = ret;
          return -1;
        }
      }
      return 0;
    }

    // not block if someone already had the lock , errno is EBUSY
    int try_acquire(void)
    {
      thread_t t_id = thread::self();
      if (thread::thr_equal(this->owner_thr_, t_id))
        ++this->nesting_level_;
      else
      {
        int ret = 0;
        if ((ret = ::pthread_mutex_trylock(&this->mutex_)) == 0)
        {
          this->owner_thr_ = t_id;
          this->nesting_level_ = 0;
        }else
        {
          errno = ret;
          return -1;
        }
      }
      return 0;
    }

    // release lock ownership 
    int release(void)
    {
      if (thread::thr_equal(thread::self(), this->owner_thr_))
      {
        if (this->nesting_level_ > 0)
          --this->nesting_level_;
        else
        {
          this->owner_thr_ = 0;
          int result = ::pthread_mutex_unlock(&this->mutex_);
          if (result != 0)
          {
            errno = result;
            return -1;
          }
          return 0;
        }
      }else
      {
        return -1;
      }
      return 0;
    }

    // return mutex_t
    const pthread_mutex_t &lock() const
    { return mutex_; }
private:
    //
    int  nesting_level_;

    // 
    thread_t owner_thr_;

    pthread_mutex_t mutex_;
  };
} // namespace ndk

#endif // NDK_RECURSIVE_THREAD_MUTEX_H_

