// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 09:50
 */
//========================================================================

#ifndef NDK_SINGLETON_H_
#define NDK_SINGLETON_H_

#include "guard.h"
#include "thread_mutex.h"

namespace ndk
{
  /**
   * @class singleton
   *
   * @brief 
   */
  template<typename T, typename M = thread_mutex>
  class singleton
  {
  public:
    static T *instance()
    {
      if (singleton<T, M>::instance_ == 0)
      {
        guard<M> g(singleton<T, M>::mutex_);
        if (singleton<T, M>::instance_ == 0)
          singleton<T, M>::instance_ = new T;
      }
      return singleton<T, M>::instance_;
    }
    
    // return old instance.
    static T *instance(T *t)
    {
      guard<M> g (singleton<T, M>::mutex_);
      T *inst = singleton<T, M>::instance_;
      singleton<T, M>::instance_ = t;
      return inst;
    }
  protected:
    singleton() 
    { }

    ~singleton() 
    { }
  private:
    static T *instance_;
    static M mutex_;
  };
  //
  template<typename T, typename M>
  T *singleton<T, M>::instance_ = 0;

  //  
  template<typename T, typename M>
  M singleton<T, M>::mutex_;
} // namespace ndk 

#endif // NDK_SINGLETON_H_

