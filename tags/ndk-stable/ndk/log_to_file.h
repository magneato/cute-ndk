// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-20 19:06
 */
//========================================================================

#ifndef NDK_LOG_TO_FILE_H_
#define NDK_LOG_TO_FILE_H_

#include "ndk/types.h"
#include "ndk/log_output.h" 
#include "ndk/log_manager.h"

namespace ndk 
{
  /**
   * @class log_to_file
   * 
   * @brief 
   */
  class log_to_file : public log_output
  {
    friend class log_manager;
  public:
    log_to_file();

    virtual ~log_to_file();

    /**
     * Open the back end object. Perform any actions needed to prepare
     * the object for later logging operations.
     *
     * @retval 0 for success.
     * @retval -1 for failure.
     */
    virtual int open(const output_entry *);

    // Close the output completely.
    virtual int close(void);

    //
    virtual int reset(void);

    // Update log strategy
    virtual void update_config(const output_entry *);

    /**
     * Process a log record.
     *
     * @retval -1 for failure; else it is customarily the number of bytes
     * processed, on zero is a no-op.
     */
    virtual int log(const char *log_msg, int len, int ll, time_t now = 0);
  protected:
    //
    int create_file(void);

    //
    void build_filename(void);
    /**
     * @retval 0 file size is not overflow.
     * @retval 1 file size is overflow and rollover the files.
     */
    int check_file_size(void);

    //
    int rolloever_files(void);

    //
    int generate_period_file(time_t now);
  private:
    ndk_handle log_handle_;

    int current_file_size_;

    time_t last_generate_period_file_time_;

    //
    std::string file_name_;

    //
    output_entry *output_entry_;

    //
    output_entry *old_output_entry_;
  };
} // namespace ndk

#endif // NDK_LOG_TO_FILE_H_

