#include "ndk/log_manager.h"
#include "ndk/log_to_stdout.h"
#include "ndk/log_to_file.h"
#include "ndk/ndk_log.h"
#include "ndk/guard.h"
#include "ndk/thread.h"

namespace ndk

{
// The members must be sort by <output_t>
static const char *output_mode[] = { "NULL", "STDOUT", "FILES" };

static const char LOG_ATTR_TYPE[] = ".__type__";
static const char LOG_ATTR_OUTPUT[] = ".__output__";
static const char LOG_ATTR_NAME[] = ".__name__";
static const char LOG_ATTR_DIR[] = ".__dir__";
static const char LOG_ATTR_SINGLE_FILE_SIZE[] = ".__single_file_size__";
static const char LOG_ATTR_ROLLOEVER[] = ".__rolloever__";
static const char LOG_ATTR_GENERATE_FILE_PERIOD[] = ".__generate_file_period__";

static const char *log_items[] =
{
  "LL_TRACE", "LL_DEBUG", "LL_WNING", "LL_ERROR", "LL_RINFO", "LL_FATAL"
};

const size_t log_item_num = sizeof(log_items)/sizeof(log_items[0]);

static const char *log_type_list[] =
{
  "SHUTDOWN",     // LT_SHUTDOWN == 1L << index
  "TRACE",	      // LT_TRACE    == 1L << index
  "DEBUG",        // LT_DEBUG    == 1L << index
  "WNING",	      // LT_WNING    == 1L << index
  "ERROR",	      // LT_ERROR    == 1L << index
  "RINFO",	      // LT_RINFO    == 1L << index
  "FATAL",        // LT_FATAL    == 1L << index
  "ALLS"
};

int log_manager::init(const char *config_name)
{
  guard<thread_mutex> guard(this->log_mutex_);
  this->last_check_time_  = ::time(0);
  if (this->config_filename_)
      delete []this->config_filename_;
  this->config_filename_  = new char[NDK_MAX_FILENAME_LEN + 1];
  ::memset(this->config_filename_, '\0', NDK_MAX_FILENAME_LEN + 1);

  ::strncpy(this->config_filename_, config_name, NDK_MAX_FILENAME_LEN);

  // access config file
  if (ndk_os::stat(this->config_filename_, &this->st_buff_) != 0)
  {
    NDK_LOG("get %s file stat error", this->config_filename_);
    return -1;
  }
  this->last_modify_time_ = this->st_buff_.st_mtime;

  if (this->load_config() != 0)
    return -1;

  return this->apply_config();
}
int log_manager::update_config(time_t now/* = 0*/)
{
  if (now == 0) now = time(0);
  if (difftime(now, this->last_check_time_) 
      < this->strategy_.check_config_interval_)
    return 0;
  // Lock  !!!
  this->last_check_time_ = now;

  // access config file
  if (ndk_os::stat(this->config_filename_, &this->st_buff_) != 0)
    return -1;

  // If config file is not modified, not change log strategy
  if (this->st_buff_.st_mtime == this->last_modify_time_)
    return 0;

  this->last_modify_time_ = this->st_buff_.st_mtime;

  // reload config file
  if (this->load_config() != 0)
    return -1;

  return this->apply_config();
}
int log_manager::load_config()
{
  if (this->ini_obj_)
  {
    iniparser_freedict(this->ini_obj_);
    this->ini_obj_ = 0;
  }
  this->ini_obj_ = iniparser_load(this->config_filename_);
  if (this->ini_obj_ == 0)
  {
    NDK_LOG("cannot parse file: %s", this->config_filename_);
    return -1;
  }
#ifdef NDK_LOG_DEBUG
  iniparser_dump(ini, stderr);
#endif

  // Global config
  if (this->load_cfg_global() != 0)
    return -1;

  // Modules
  if (this->load_cfg_root() != 0)
    return -1;

#ifdef NDK_LOG_DEBUG
  this->dump_cfg();
#endif
}
int log_manager::load_cfg_global()
{
  // 1. check_config_interval
  char *interval = iniparser_getstring(this->ini_obj_, 
                                       "check_config_interval", 
                                       0);
  int value = 0;
  if (interval && ::strlen(interval) > 0)
  {
    value = ::atoi(interval);
    if (::strchr(interval, 'm') || ::strchr(interval, 'M'))
      value *= 60;
  }
  if (value <= 0)
  {
    value = 5;
    NDK_LOG("You didn't config <check_config_interval>, so use"
            " default value <%dsec>", value);
  }
  this->strategy_.check_config_interval_ = value;

  // 2. max_len_of_one_record
  int limit = iniparser_getint(this->ini_obj_, 
                               "max_len_of_one_record", 
                               512);
  if (limit <= 0) limit = 512;
  // Apply config
  this->max_len_of_one_record_ = limit;

  return 0;
}
int log_manager::load_cfg_root()
{
  this->root_entry_ = this->get_module_cfg_entry(ROOT_MODULE_NAME);
  if (this->root_entry_ == 0) return -1;
  return 0;
}
int log_manager::apply_config()
{
  ndk_log *l = this->root_node_;
  while (l)
  {
    config_entry *entry = this->get_module_cfg_entry(l->module_name());
    l->apply_module_cfg(entry);
    l = l->next();
  }

  return 0;
}
config_entry *log_manager::get_module_cfg_entry(const char *mname)
{
  config_entry *entry = new config_entry;
  // 1. log type
  int type = this->get_log_type(mname);
  int parent_type = this->get_parent_log_type(mname);
  entry->log_type_ = type | parent_type;

  // 2. log ouput
  int output = this->get_log_output();
  entry->output_ = output;

  // 3. log dir/name
  char *dir = iniparser_getstring(this->ini_obj_, 
                                  LOG_ATTR_DIR,
                                  0);
  if (dir && ::strlen(dir) > 0)
  {
    ::strncpy(entry->log_dir_, dir, NDK_MAX_PATH_LEN);
  }else
  {
    ::strcpy(entry->log_dir_, "./");
  }
  char *fname = iniparser_getstring(this->ini_obj_, 
                                    LOG_ATTR_NAME,
                                    0);
  if (fname && ::strlen(fname) > 0)
  {
    ::strncpy(entry->file_name_, fname, NDK_MAX_FILENAME_LEN);
  }else
  {
    ::strcpy(entry->file_name_, "log");
  }
  
  // 4. single file size
  char *fsize = iniparser_getstring(this->ini_obj_, 
                                    LOG_ATTR_SINGLE_FILE_SIZE,
                                    0);
  int value = 0;
  if (fsize && ::strlen(fsize) > 0)
  {
    value = ::atoi(fsize);
    if (::strrchr(fsize, 'k') || ::strrchr(fsize, 'K'))
      value *= 1024;
    else
      value *= 1024 * 1024;
  }
  // Default is 1M
  if (value <= 0)
    value = 1 * 1024 * 1024;
  entry->single_file_size_ = value;

  // 5. rolloever index
  entry->rolloever_ = iniparser_getint(this->ini_obj_,
                                       LOG_ATTR_ROLLOEVER,
                                       10);

  // 6. generate file period
  entry->generate_file_period_ = iniparser_getint(this->ini_obj_,  
                                                  LOG_ATTR_GENERATE_FILE_PERIOD,
                                                  0);
  return 0;
}
int log_manager::get_log_type(const char *mname)
{
  char name[MAX_MODULE_NAME_LEN + 1] = {0};
  ::strncat(name, mname, MAX_MODULE_NAME_LEN);
  ::strncat(name, LOG_ATTR_TYPE, sizeof(LOG_ATTR_TYPE) - 1);
  char *log_type = iniparser_getstring(this->ini_obj_, name, 0);
  int type = 0;
  if (log_type && ::strlen(log_type) > 0)
  {
    if (::strstr(log_type, "ALLS"))
      NDK_SET_BITS(level, LL_ALLS);
    for (size_t j = 0;
         j < sizeof(log_type_list) / sizeof(log_type_list[0]);
         ++j)
    {
      if (::strcmp(log_type_list[j], "ALLS") != 0
          && ::strstr(log_type, log_type_list[j]))
      {
        NDK_SET_BITS(type, 1L << j);
      }
    }
  }
  return type;
}
int log_manager::get_parent_log_type(const char *mname, char *parent)
{
  if (str == 0) return 0;
  char *pos = ::strrchr(str, '.');
  int l = 0;
  l = this->get_log_type(str);
  if (l == -1) l = 0;
  char s[MAX_MODULE_NAME_LEN + 1] = {0};
  if (pos) // grade
  {
    ::strncpy(parent, str, pos - str);
#ifdef NDK_LOG_DEBUG
    fprintf(stderr, "parent = %s\n", parent);
#endif
    return l | this->get_parent_log_type(parent, s);
  }
  return l;
}
int log_manager::get_log_output()
{
  char *output = iniparser_getstring(this->ini_obj_, 
                                     LOG_ATTR_OUTPUT,
                                     0);
  int value = 0;
  if (output && ::strlen(output) > 0)
  {
    for (size_t i = 0;
         i < sizeof(output_mode) / sizeof(output_mode[0]);
         ++i)
      if (::strstr(output, output_mode[i]))
        NDK_SET_BITS(value, 1L << i);
  }
  return value;
}
ndk_log *log_manager::get_logger(const char *module_name)
{
  if (module_name == 0 || ::strlen(module_name) == 0)
    return 0;

  char name[MAX_MODULE_NAME_LEN + 1] = {0};
  char *p = name;
  if (::strstr(module_name, ROOT_MODULE_NAME) == 0)
  {
    ::strcpy(p, ROOT_MODULE_NAME".");
    ::strncat(p, module_name, MAX_MODULE_NAME_LEN);
  }else
  {
    p = const_cast<char *>(module_name);
  }
  // Lock here
  guard<thread_mutex> guard(this->log_mutex_);
  // Find exist logger first.
  ndk_log *itor = this->root_node_;
  while (itor)
  {
    if (strcmp(itor->module_name(), p) == 0)
    {
      return itor;
    }
    itor = itor->next();
  }

  ndk_log *l = new ndk_log(this, p);
  if (l == 0) return 0;
  config_entry *entry = this->get_module_cfg_entry(p);
  l->apply_module_config(entry);
  this->insert_logger(l);
  return l;
}
int log_manager::insert_logger(ndk_log *l)
{
  ndk_log *itor = this->root_node_;
  if (itor == 0)
  {
    this->root_node_ = l;
    return 0;
  }
  while (itor->next()) itor = itor->next();

  itor->next(l);
  return 0;
}
namespace log_help_
{
  static int get_bit(int n)
  {
    for (size_t i = 1; i < sizeof(int) * 8; ++i)
    {
      if ((n >> i) & 0x0001)
        return i;
    }
    return 0;
  }
}
int log_manager::log(ndk_log *log, const char *record, const int len)
{
  // Lock from here !!
  guard<thread_mutex> guard(this->log_mutex_);
  this->update_config();

  return log->log(record, len);
}
int log_manager::log(ndk_log *log, 
                     int ll, 
                     const char *format,
                     va_list &va_ptr)
{
  // Lock from here !!
  guard<thread_mutex> guard(this->log_mutex_);
  this->update_config();

  if (log->shutdown() || !log->enable_for(ll))
    return 0;

  // Format
  this->current_time_.gettimeofday();
  this->current_dtime_.update(this->current_time_.sec());
  this->current_dtime_.to_str(this->log_time_, 
                              sizeof(this->log_time_));
  this->current_time_.sec(0);
  int len = 0;
  int bit = log_help_::get_bit(ll);
  if (bit == 0)
  {
    NDK_LOG("The parameter log-level(%d) is invalid", ll);
    return 0;
  }
  ll =(log_help_::get_bit(ll) - 1) % log_item_num;
  //::memset(this->log_record_, '\0', 
           //this->lrecord_cfg_.max_len_of_one_record_);
  int ret = ndk_os::snprintf(this->log_record_, 
                             this->lrecord_cfg_.max_len_of_one_record_ + 1, 
                             "[%s.%03lu][%lu]<%s><%s>: ",

                             this->log_time_,
                             this->current_time_.msec(),
                             thread::self(),
                             log_items[ll],
                             log->module_name() + sizeof(ROOT_MODULE_NAME));
  if (ret >= this->lrecord_cfg_.max_len_of_one_record_ + 1)
    ret = this->lrecord_cfg_.max_len_of_one_record_;
  len += ret;
  if (len < this->lrecord_cfg_.max_len_of_one_record_)
  {
    ret = ndk_os::vsnprintf(this->log_record_ + len, 
                            this->lrecord_cfg_.max_len_of_one_record_ + 1 - len,
                            format,
                            va_ptr);
    //va_end(va_ptr);
    if (ret < 0) return -1;
    /**
     * check overflow or not
     * Note : snprintf and vnprintf return value is the number of characters
     *(not including the trailing ’\0’) which would have been  written  to 
     * the  final  string  if enough space had been available.
     */
    if (ret >= (this->lrecord_cfg_.max_len_of_one_record_ + 1 - len))
      ret = this->lrecord_cfg_.max_len_of_one_record_ + 1 - len - 1;
    // vsnprintf return the length of <va_ptr> actual
    len += ret;
  }
  this->log_record_[len] = '\n';
  this->log_record_[len + 1] = '\0';
  //
  return log->log(this->log_record_, len + 1, ll);;
}
} // namespace ndk

