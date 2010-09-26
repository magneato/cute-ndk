// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-04 10:28
 */
//========================================================================

#ifndef NDK_STRACE_H_
#define NDK_STRACE_H_

#include <cstdio>
#include <stdint.h>
#include <cstring>

#include "ndk/guard.h"
#include "ndk/thread.h"
#include "ndk/date_time.h"
#include "ndk/time_value.h"
#include "ndk/thread_mutex.h"

namespace ndk
{
  /**
   * @class strace
   * 
   * @brief 
   */
  class strace
  {
  public:
    strace(const char *file, const char *func, int line)
    : line_(line),
    file_(file),
    func_(func)
    {
      guard<thread_mutex> g(strace::mutex_);
      if (strace::count_ >= sizeof(this->spaces_) / 2) --strace::count_;
      std::memset(this->spaces_, ' ', strace::count_ * 2);
      *(this->spaces_ + strace::count_ * 2) = '\0';
      this->current_time_.update();
      this->current_dtime_.update(this->current_time_.sec());
      this->current_dtime_.time_to_str(this->log_time_,
                                       sizeof(this->log_time_));
      this->current_time_.sec(0);
      std::fprintf(stderr, 
                   "<%s.%03lu>: %s(%d)<%lu>calling `%s` in file `%s` on line `%d`\n",
                   this->log_time_,
                   this->current_time_.msec(),
                   this->spaces_, 
                   strace::count_++, 
                   (unsigned long int)thread::self(),
                   this->func_, 
                   this->file_, 
                   this->line_);
    }
    //
    ~strace()
    {
      guard<thread_mutex> g(strace::mutex_);
      if (strace::count_ >= sizeof(this->spaces_) / 2) --strace::count_;
      std::memset(this->spaces_, ' ', (strace::count_ - 1) * 2);
      *(this->spaces_ + (strace::count_ - 1) * 2) = '\0';
      this->current_time_.update();
      this->current_dtime_.update(this->current_time_.sec());
      this->current_dtime_.time_to_str(this->log_time_,
                                       sizeof(this->log_time_));
      this->current_time_.sec(0);
      std::fprintf(stderr, "<%s.%03lu>: %s(%d)<%lu>leaving `%s` in file `%s`\n", 
                   this->log_time_,
                   this->current_time_.msec(),
                   this->spaces_, 
                   --strace::count_, 
                   (unsigned long int)thread::self(), 
                   this->func_, 
                   this->file_);
    }
  private:
    int           line_;
    const  char*  file_;
    const  char*  func_;

    static unsigned int count_;
    char          log_time_[32];
    char          spaces_[128];
    date_time     current_dtime_;
    time_value    current_time_;
    static thread_mutex mutex_;
  };

} // namespace ndk

#endif // NDK_STRACE_H_

