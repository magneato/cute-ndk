// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-20 19:06
 */
//========================================================================

#ifndef NDK_LOG_TO_FILE_H_
#define NDK_LOG_TO_FILE_H_

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
    virtual int open(const char *logger_arg,
                     log_manager::strategy *);

    // Close the output completely.
    virtual int close(void);

    //
    virtual void reset(void);

    // Update log strategy
    virtual void update_strategy(const char *, log_manager::strategy *ls);

    /**
     * Process a log record.
     *
     * @retval -1 for failure; else it is customarily the number of bytes
     * processed, on zero is a no-op.
     */
    virtual int log(const char *log_msg, int len, int ll);
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
    void generate_new_file(void);

    //
    void calc_next_time(void);
  private:
    ndk_handle log_handle_;

    int      current_file_size_;

    //
    time_t   day_start_time_;

    //
    char *file_name_;

    char *log_dir_;

    //
    log_manager::strategy *log_strategy_;

    //
    log_manager::strategy *old_log_strategy_;

    //
    date_time last_calc_dtime_;
  };
} // namespace ndk

#endif // NDK_LOG_TO_FILE_H_

