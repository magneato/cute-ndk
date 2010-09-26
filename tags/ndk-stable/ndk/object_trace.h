// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-08-20 16:41
 */
//========================================================================

#ifndef NDK_OBJECT_TRACE_H_
#define NDK_OBJECT_TRACE_H_

#include "ndk/guard.h"
#include "ndk/thread.h"

namespace ndk
{
  /**
   * @class object_trace
   *
   * @brief 
   */
  template <typename object>
  class object_trace
  {
  public:
    object_trace()
    { ++object_trace<object>::malloc_count_; }

    //
    ~object_trace()
    { --object_trace<object>::malloc_count_; }

    static int malloc_count()
    { return object_trace<object>::malloc_count_; }
  protected:
    static int malloc_count_;
  };

  template<typename object>
    int object_trace<object>::malloc_count_ = 0;

  // thread safe
  template <typename object>
  class object_trace_mt
  {
  public:
    object_trace_mt()
    { 
      ndk::guard<ndk::thread_mutex> g(object_trace_mt<object>::mutex_);
      ++object_trace_mt<object>::malloc_count_;
    }

    //
    ~object_trace_mt()
    { 
      ndk::guard<ndk::thread_mutex> g(object_trace_mt<object>::mutex_);
      --object_trace_mt<object>::malloc_count_;
    }

    static int malloc_count()
    { return object_trace_mt<object>::malloc_count_; }
  protected:
    static int malloc_count_;
    static ndk::thread_mutex mutex_;
  };

  template<typename object>
    int object_trace_mt<object>::malloc_count_ = 0;

  template<typename object>
    ndk::thread_mutex object_trace_mt<object>::mutex_;
} // namespace ndk

#endif // NDK_OBJECT_TRACE_H_
