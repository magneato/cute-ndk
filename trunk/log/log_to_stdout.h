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
    log_to_stdout();

    virtual ~log_to_stdout();

    // Initialize the event logging facility.
    //
    // $param logger_arg No used.
    virtual int open(const char *logger_arg, log_manager::strategy *);

    // No-op.
    virtual int close();

    virtual void reset(void);

    // Update log strategy
    virtual void update_strategy(const char *, log_manager::strategy *ls);

    // Print a log message to stdout
    virtual int log(const char *log_msg, int len, int ll);
  };
} // namespace ndk

#endif // NDK_LOG_TO_STDOUT_H_

