// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-20 19:06
 */
//========================================================================

#ifndef NDK_LOG_TO_STDOUT_H_
#define NDK_LOG_TO_STDOUT_H_

#include "ndk/log_output.h"
#include "logger.h"

namespace ndk
{
  /**
   * @class log_to_stdout
   * 
   * @brief Implements an log_output that logs to the stdout.
   */
  class log_to_stdout : public log_output
  {
  public:
    log_to_stdout()
    { type_ = output_entry::om_stdout; }

    virtual ~log_to_stdout() { }

    // Initialize the event logging facility.
    //
    // $param logger_arg No used.
    virtual int open(const output_entry *)
    { setbuf(stdout, 0); return 0; }

    // No-op.
    virtual int close()
    { return 0; }

    virtual int reset(void)
    { return 0; }

    // Update log strategy
    virtual void update_config(const output_entry *) { }

    // Print a log message to stdout
    virtual int log(const char *log_msg, int , int ll, time_t = 0)
    {
      if (ll & LL_DEBUG)
        return ::fprintf(stdout, "\033[0;33m%s\033[0m", log_msg);
      else if (ll & LL_TRACE)
        return ::fprintf(stdout, "%s", log_msg);
      else if (ll & LL_RINFO)
        return ::fprintf(stdout, "\033[0;32m%s\033[0m", log_msg);
      else if (ll & LL_ERROR)
        return ::fprintf(stdout, "\033[1;31m%s\033[0m", log_msg);
      else if (ll & LL_WNING)
        return ::fprintf(stdout, "\033[0;35m%s\033[0m", log_msg);
      else if (ll & LL_FATAL)
        return ::fprintf(stdout, "\033[4;36m%s\033[0m", log_msg);
      return ::fprintf(stdout, "%s", log_msg);
    }
  };
} // namespace ndk

#endif // NDK_LOG_TO_STDOUT_H_

