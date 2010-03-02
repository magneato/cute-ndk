// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-20 19:05
 */
//========================================================================

#ifndef NDK_LOG_MANAGER_H_
#define NDK_LOG_MANAGER_H_

#include "ndk/config.h"
#include "ndk/singleton.h"
#include "ndk/date_time.h"
#include "ndk/time_value.h"
extern "C"
{
# include "ndk/iniparser.h"
}
#include "ndk/thread_mutex.h"

namespace ndk
{
  //
# define MAX_MODULE_NAME_LEN      255

  //
# define ROOT_MODULE_NAME         "root"

  // Generate file period stategy
  enum
  {
    gp_disable  = 0x00,
    gp_hourly	  = 0x11,
    gp_daily    = 0x12,
    gp_weekly	  = 0x13
  };
  /**
   * @class config_entry
   *
   */
  class config_entry
  {
  public:
    config_entry()
      : log_type_(0),
      output_(OUTPUT_FILES),
      single_file_size_(512*1024),
      rolloever_(5),
      generate_file_period_(0),
      file_name_(0),
      log_dir_(0)
    {
      file_name_ = new char[NDK_MAX_FILENAME_LEN + 1];
      ::memset(file_name_, 0, NDK_MAX_FILENAME_LEN + 1);
      log_dir_ = new char[NDK_MAX_PATH_LEN + 1];
      ::memset(log_dir_, 0, NDK_MAX_PATH_LEN + 1);
    }
    ~config_entry()
    {
      if (this->file_name_) delete []this->file_name_;
      this->file_name_ = 0;
      if (this->log_dir_) delete []this->log_dir_;
      this->log_dir_ = 0;
    }

    int log_type_;

    int output_;

    int single_file_size_;

    int rolloever_;

    int generate_file_period_;

    char *file_name_;
  };
  /**
   * @class log_manager
   * 
   * @brief 
   */
  class log_manager : public singleton<log_manager>
  {
    friend class singleton<log_manager>;
  public:
    enum
    {
      om_null     = 1L << 0,
      om_stdout   = 1L << 1,
      om_files    = 1L << 2
    };

    int init(const char *config_name);

    // 
    ndk_log *get_logger(const char *module_name);

    // log
    int log(ndk_log *log, int ll, const char *format, va_list &va_ptr);

    int log(ndk_log *log, const char *record, const int len);

  protected:
    log_manager()
      : config_filename_(0),
      log_record_(0),
      root_node_(0),
    { }

    ~log_manager()
    {
      if(this->config_filename_)
        delete []this->config_filename_;
      this->config_filename_ = 0;
    }
    //
    int update_config(void);

    // Apply new configure.
    int apply_config(void);

    //
    int load_config(void);

    //
    int get_parent_log_type(const char *str, char *parent);

    //
    int insert_logger(ndk_log *l);

    //
    void dump_cfg(void);
  private:
    // Store config file status
    ndk_stat st_buff_;

    //
    time_t last_check_time_;

    //
    time_t last_modify_time_;

    char *config_filename_;

    // One log record buffer
    char *log_record_;

    ndk_log *root_node_;

    config_entry *root_entry_;

    dictionary *ini_obj_;

    // Time
    time_value current_time_;

    //
    date_time  current_dtime_;

    thread_mutex log_mutex_;

    char log_time_[64];
  };
} // namespace ndk

#endif // NDK_LOG_MANAGER_H_

