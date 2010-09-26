// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-20 19:05
 */
//========================================================================

#ifndef NDK_LOG_OUTPUT_H_
#define NDK_LOG_OUTPUT_H_

#include "ndk/log_manager.h"

namespace ndk
{
  /**
   * @class log_output
   * 
   * @brief Defines the interface for ndk_log output processing.
   *
   * Users can derive classes from log_output for use as a custom ndk_log
   * output.
   */
  class log_output
  {
  public:
    // No-op virtual destructor.
    virtual ~log_output() { }

    /**
     * Open the back end object. Perform any actions needed to prepare
     * the object for later logging operations.
     *
     * @retval 0 for success.
     * @retval -1 for failure.
     */
    virtual int open(const char *logger_arg,
	    log_manager::strategy *) = 0;

    // Close the output completely.
    virtual int close(void) = 0;

    // Reset the strategy.
    virtual void reset(void) = 0;

    // Update log strategy
    virtual void update_strategy(const char *, log_manager::strategy *ls) = 0;

    /**
     * Process a log record.
     *
     * @retval -1 for failure; else it is customarily the number of bytes
     * processed, on zero is a no-op.
     */
    virtual int log(const char *log_msg, int len, int ll) = 0;
  };
} // namespace ndk

#endif // NDK_LOG_OUTPUT_H_

