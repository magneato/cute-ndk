// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-20 19:05
 */
//========================================================================

#ifndef NDK_LOG_MANAGER_H_
#define NDK_LOG_MANAGER_H_

#include "singleton.h"
#include "date_time.h"
#include "time_value.h"
extern "C"
{
# include "iniparser.h"
}
#include "thread_mutex.h"

#include <map>
#include <deque>
#include <string>
#include <sys/stat.h>

namespace ndk
{
  //
# define MAX_MODULE_NAME_LEN      255

  /**
   * @class module_entry
   *
   */
  class module_entry
  {
    friend class log_manager;
  public:
    module_entry()
      : log_type_(0),
      max_len_of_one_record_(511)
    { }
    ~module_entry()
    { }

    inline module_entry &operator = (const module_entry &ce)
    {
      if (&ce == this) return *this;
      this->log_type_              = ce.log_type_;
      this->max_len_of_one_record_ = ce.max_len_of_one_record_;
      this->output_list_           = ce.output_list_;
      return *this;
    }
    inline bool operator == (const module_entry &ce)
    {
      if (&ce == this) return true;
      if (this->output_list_ == ce.output_list_)
        return true;
      return false;
    }

    inline bool operator != (const module_entry &ce)
    {
      return !(*this == ce);
    }

  public:
    typedef std::deque<std::string> output_list_t;
    typedef std::deque<std::string>::iterator output_list_iter;

  public:
    int log_type_;

    int max_len_of_one_record_;

    output_list_t output_list_;
  };
  /**
   * @class log_manager
   * 
   * @brief 
   */
  class logger;
  class log_output;
  class log_manager : public singleton<log_manager>
  {
    friend class singleton<log_manager>;
    friend class logger;
  public:
    int init(const char *config_name);

    // 
    logger *get_logger(const char *module_name);

  protected:
    // log
    int log(logger *log, int ll, const char *format, va_list &va_ptr);

    int log(logger *log, const char *record, const int len);

  protected:
    log_manager()
      : check_config_interval_(10),
      max_len_of_one_record_(512),
      log_record_(0),
      root_node_(0),
      ini_obj_(0)
    { }

    ~log_manager()
    { }

    //
    int update_config(time_t now = 0);

    logger *get_logger_i(const char *module_name);

    // Apply new configure.
    int apply_config(void);

    //
    int load_config(void);

    //
    void check_unused_output();

    //
    void build_record_buffer(void);

    //
    int load_cfg_global(void);

    //
    int load_cfg_modules(void);

    //
    module_entry *get_module_cfg_entry(const char *);

    //
    std::deque<log_output *> get_output_list(module_entry *);

    //
    char *get_config_attribe(const char *, const char *, const char *);

    //
    char *get_parent_value(const char *mname,
                           char *(log_manager::*func)(const char *));

    //
    int get_log_type(const char *);

    //
    int get_parent_log_type(const char *str, char *parent);

    //
    std::deque<std::string> get_log_output(const char *);

    //
    std::deque<std::string> get_parent_output(const char *str);

    //
    char *get_log_max_len_of_one_record(const char *);

    //
    char *get_parent_max_len_of_one_record(const char *str);

    //
    int insert_logger(logger *l);

    //
    void dump_cfg(void);
  private:
    //
    int check_config_interval_;

    int max_len_of_one_record_;

    //
    time_t last_check_time_;

    //
    time_t last_modify_time_;

    // One log record buffer
    char *log_record_;

    logger *root_node_;

    dictionary *ini_obj_;

    // Time
    time_value current_time_;

    //
    date_time  current_dtime_;

    thread_mutex log_mutex_;

    std::string config_filename_;

    // Store config file status
    struct stat st_buff_;

    typedef std::map<std::string, log_output *> output_list_t;
    typedef std::map<std::string, log_output *>::iterator output_list_iter;

    output_list_t output_list_;

    char log_time_[64];
  };
} // namespace ndk

#endif // NDK_LOG_MANAGER_H_

