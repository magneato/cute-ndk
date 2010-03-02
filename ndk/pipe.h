// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-20 16:36
 */
//========================================================================

#ifndef NDK_PIPE_H_
#define NDK_PIPE_H_

#include <unistd.h>
#include "ndk/types.h"

namespace ndk
{
  /**
   * @class pipe
   * 
   * @brief 
   */
  class pipe
  {
  public:
    pipe()
    {
      handles_[0] = NDK_INVALID_HANDLE;
      handles_[1] = NDK_INVALID_HANDLE;
    }

    virtual ~pipe() { this->close(); }

    //
    int open(void) { return ::pipe (this->handles_); }

    //
    int close(void)
    {
      int result = 0;
      if (this->handles_[0] != NDK_INVALID_HANDLE)
        result = ::close(this->handles_[0]);
      if (this->handles_[1] != NDK_INVALID_HANDLE)
        result |= ::close(this->handles_[1]);

      handles_[0] = handles_[1] = NDK_INVALID_HANDLE;
      return result;
    }
    //
    ndk_handle read_handle() const
    { return this->handles_[0]; }
    
    // 
    ndk_handle write_handle() const
    { return this->handles_[1]; }

private:
    ndk_handle handles_[2];
  };
} // namespace ndk

#endif // NDK_PIPE_H_

