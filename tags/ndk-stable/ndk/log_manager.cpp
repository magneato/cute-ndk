#include "log_manager.h"
#include "log_to_stdout.h"
#include "log_to_file.h"
#include "logger.h"
#include "guard.h"

#include <deque>
#include <string>
#include <cassert>
#include <iterator>
#include <iostream>
#include <algorithm>

namespace ndk

{
// The members must be sort by <output_t>
static const char *output_modes[] = { "SHUTDOWN", "STDOUT", "FILES" };

//
static const char GLOBAL_SECTION[]                    = "global";
static const char MODULES_SECTION[]                   = "modules";
static const char ROOT_MODULE_NAME[]                  = "root";
static const char LOG_ATTR_TYPE[]                     = "__type__";
static const char LOG_ATTR_OUTPUT[]                   = "__output__";
static const char LOG_ATTR_FILE_OUTPUT[]              = "__file_output__";
static const char LOG_ATTR_MAX_LEN_OF_ONE_RECORD[]    = "__max_len_of_one_record__";

static const char LOG_OUTPUT_TYPE[]                   = "type";
static const char LOG_OUTPUT_NAME[]                   = "name";
static const char LOG_OUTPUT_DIR[]                    = "dir";
static const char LOG_OUTPUT_SINGLE_FILE_SIZE[]       = "single_file_size";
static const char LOG_OUTPUT_ROLLOEVER[]              = "rolloever";
static const char LOG_OUTPUT_GENERATE_FILE_PERIOD[]   = "generate_file_period";

static const char *log_types[] =
{
  "LL_TRACE", "LL_DEBUG", "LL_WNING", "LL_ERROR", "LL_RINFO", "LL_FATAL"
};

const size_t log_types_size = sizeof(log_types)/sizeof(log_types[0]);

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
  if (config_name == 0 || config_name[0] == '\0')
    return -1;
  guard<thread_mutex> guard(this->log_mutex_);
  this->last_check_time_  = ::time(0);
  this->config_filename_ = config_name;

  // access config file
  if (::stat(this->config_filename_.c_str(), &this->st_buff_) != 0)
  {
    ::fprintf(stderr, "get %s file stat error", 
              this->config_filename_.c_str());
    return -1;
  }

  this->last_modify_time_ = this->st_buff_.st_mtime;

  if (this->load_config() != 0)
    return -1;

  int result = this->apply_config();

  this->dump_cfg();

  this->build_record_buffer();

  return result;
}
int log_manager::update_config(time_t now/* = 0*/)
{
  if (now == 0) now = ::time(0);
  if (difftime(now, this->last_check_time_) 
      < this->check_config_interval_)
    return 0;
  // Lock  !!!
  this->last_check_time_ = now;

  // access config file
  if (::stat(this->config_filename_.c_str(), 
             &this->st_buff_) != 0)
    return -1;

  // If config file is not modified, not change log strategy
  if (this->st_buff_.st_mtime == this->last_modify_time_)
    return 0;

  this->last_modify_time_ = this->st_buff_.st_mtime;

  // reload config file
  if (this->load_config() != 0)
    return -1;

  this->check_unused_output();

  int result = this->apply_config();

  this->dump_cfg();

  this->build_record_buffer();

  return result;
}
void log_manager::build_record_buffer()
{
  logger *l = this->root_node_;
  while (l)
  {
    if (l->module_entry_)
    {
      if (this->max_len_of_one_record_ < l->module_entry_->max_len_of_one_record_)
        this->max_len_of_one_record_ = l->module_entry_->max_len_of_one_record_;
    }
    l = l->next();
  }
  if (this->log_record_)
    delete []this->log_record_;
  this->log_record_ = 0;
  this->log_record_ = new char[this->max_len_of_one_record_ + 1];
  ::memset(this->log_record_, 0, this->max_len_of_one_record_ + 1);
  this->max_len_of_one_record_ -= 1;
}
int log_manager::load_config()
{
  if (this->ini_obj_)
  {
    iniparser_freedict(this->ini_obj_);
    this->ini_obj_ = 0;
  }
  this->ini_obj_ = iniparser_load(this->config_filename_.c_str());
  if (this->ini_obj_ == 0)
  {
    ::fprintf(stderr, "cannot parse file: %s", 
              this->config_filename_.c_str());
    return -1;
  }
#ifdef NDK_LOG_DEBUG
  iniparser_dump(this->ini_obj_, stderr);
#endif

  // Global config
  if (this->load_cfg_global() != 0)
    return -1;

  if (this->load_cfg_modules() != 0)
    return -1;

  return 0;
}
int log_manager::load_cfg_global()
{
  // 1. check_config_interval
  char *interval = this->get_config_attribe(GLOBAL_SECTION,
                                            "check_config_interval",
                                            0);
  int value = 0;
  if (interval)
  {
    value = ::atoi(interval);
    if (::strchr(interval, 'm') || ::strchr(interval, 'M'))
      value *= 60;
  }
  if (value <= 0)
  {
    value = 10;
    ::fprintf(stderr, "You didn't config <check_config_interval>, so use"
              " default value <%dsec>", 
              value);
  }
  this->check_config_interval_ = value;

  return 0;
}
int log_manager::load_cfg_modules()
{
  return 0;
}
void log_manager::check_unused_output()
{
  log_manager::output_list_iter p = this->output_list_.begin();
  for (; p != this->output_list_.end();)
  {
    const char *section = p->first.c_str();
    char *value = this->get_config_attribe(section,
                                           LOG_OUTPUT_TYPE, 
                                           0);
    int type = output_entry::om_shutdown;
    if (value) 
    {
      for (size_t i = 0; i < sizeof(output_modes)/sizeof(output_modes[0]); ++i)
      {
        if (::strcasecmp(value, output_modes[i]) == 0)
        {
          type = 1L << i;
          break;
        }
      }
    }
    if (type == output_entry::om_shutdown
        || type != p->second->type())
    {
      delete p->second;
      this->output_list_.erase(p++);
    }else
      ++p;
  }
}
int log_manager::apply_config()
{
  logger *l = this->root_node_;
  while (l)
  {
    module_entry *entry = this->get_module_cfg_entry(l->module_name());
    l->apply_module_config(entry);
    l = l->next();
  }

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
  static char *strstrip(const char *in_str, char *out_str, int len)
  {
    const char *last  = 0;
    const char *begin = 0;

    if (in_str == 0 || out_str == 0 || len <= 0) 
      return 0;

    while (isspace((int)*in_str) && *in_str) 
      in_str++;
    begin = in_str;

    for (; *in_str; ++in_str)
    {
      if (isspace((int)*in_str) == 0)
        last = in_str;
    }
    int l = last - begin + 1;
    if (l < len)
      len = l;
    ::strncpy(out_str, begin, len);
    return out_str ;
  }
  static void get_token_list (const char *buff, char sep, 
                              std::deque<std::string> &token_v)
  {
    const char *pos = buff;
    while (*pos)
    {
      const char *token_begin = pos;
      const char *token_pos = pos;
      if (*pos == sep && pos == buff)  // the first letter is 'sep'
      {
        token_v.push_back ("");
        if (*(pos + 1) == sep || *(pos + 1) == 0)
        {
          token_v.push_back ("");
        }
        ++pos;
        continue;
      }else if (*pos != sep)// key words
      {
        for (; *token_pos != sep && *pos; ++token_pos, ++pos)
          ;
        std::string is(token_begin, token_pos - token_begin);
        if (*token_begin == ' ' || *(token_pos - 1) == ' ')
        {
          char *s = new char[is.length()];
          ::memset(s, 0, is.length());
          strstrip(is.c_str(), s, static_cast<int>(is.length()));
          if (::strlen(s) > 0)
            is.assign(s);
          delete []s;
        }
        token_v.push_back (is);
        if (*pos == 0)
          break;
      }else
      {
        if (*(pos + 1) == 0 || *(pos + 1) == sep) // the last letter is 'sep'
        {
          token_v.push_back ("");
        }
        ++pos; // skip 'sep'
      }
    } // end of `while (*pos)'
  }
}
module_entry *log_manager::get_module_cfg_entry(const char *mname)
{
#ifdef NDK_LOG_DEBUG
  ::fprintf(stderr, "get module %s\n", mname);
#endif
  module_entry *entry = new module_entry();
  if (this->ini_obj_ == 0) return entry;

  // 1. log type
  int type = this->get_log_type(mname);
  entry->log_type_ = type;
  int parent_type = 0;
  if (::strstr(mname, ".") != 0)
  {
    char pname[MAX_MODULE_NAME_LEN + 1] = {0};
    parent_type = this->get_parent_log_type(mname, pname);
    entry->log_type_ |= parent_type;
  }

  // 2. log ouput
  std::deque<std::string> ot = this->get_log_output(mname);
  std::deque<std::string> neg;
  std::deque<std::string>::iterator ot_itor;
  for (ot_itor = ot.begin(); ot_itor != ot.end(); ++ot_itor)
  {
    if (!ot_itor->empty() && (*ot_itor)[0] == '~')
      neg.push_back(*ot_itor);
  }
  entry->output_list_ = ot;
  if (::strstr(mname, ".") != 0)
  {
    ot = this->get_parent_output(mname);
    std::deque<std::string>::iterator pos;
    for (pos = ot.begin(); pos != ot.end();)
    {
      if (!pos->empty() && (*pos)[0] == '~')
        pos = ot.erase(pos);
      else
        ++pos;
    }
    std::copy(ot.begin(), ot.end(), std::back_inserter(entry->output_list_));
  }
  std::sort(entry->output_list_.begin(), entry->output_list_.end());
  entry->output_list_.assign(entry->output_list_.begin(),
                             std::unique(entry->output_list_.begin(), 
                                         entry->output_list_.end()));
  std::deque<std::string>::iterator neg_itor;
  for (neg_itor = neg.begin(); neg_itor != neg.end(); ++neg_itor)
  {
    std::string s = neg_itor->substr(1);
    if (s.empty()) continue;
    std::deque<std::string>::iterator pos = std::find(entry->output_list_.begin(),
                                                      entry->output_list_.end(),
                                                      s);
    entry->output_list_.erase(pos);
  }

  // 3. max length of one log record. 
  char *max_len_of_one_record = this->get_log_max_len_of_one_record(mname);
  if (max_len_of_one_record == 0)
  {
    if (::strstr(mname, ".") != 0)
      max_len_of_one_record = this->get_parent_max_len_of_one_record(mname);
  }
  if (max_len_of_one_record)
    entry->max_len_of_one_record_ = ::atoi(max_len_of_one_record);
  else
    entry->max_len_of_one_record_ = 511;

  return entry;
}
std::deque<log_output *> log_manager::get_output_list(module_entry *me)
{
  std::deque<log_output *> output_objs;
  module_entry::output_list_iter iter = me->output_list_.begin();
  char *value = 0;
  int fsize = 0;
  for (; iter != me->output_list_.end(); ++ iter)
  {
    if (iter->empty()) continue;
    const char *section = iter->c_str();
    output_entry oe;
    // 1. type
    value = this->get_config_attribe(section,
                                     LOG_OUTPUT_TYPE, 
                                     0);
    if (value) 
    {
      for (size_t i = 0; i < sizeof(output_modes)/sizeof(output_modes[0]); ++i)
      {
        if (::strcasecmp(value, output_modes[i]) == 0)
        {
          oe.output_type_ = 1L << i;
          break;
        }
      }
    }
    if (oe.output_type_ == output_entry::om_shutdown)
      continue;
    if (oe.output_type_ == output_entry::om_stdout)
      goto OPEN;

    // 2. log dir
    value = this->get_config_attribe(section,
                                     LOG_OUTPUT_DIR, 
                                     0);
    if (value)
      oe.log_dir_ = value;
    else
      oe.log_dir_ = "./";

    // 2. log file name
    value = this->get_config_attribe(section,
                                     LOG_OUTPUT_NAME, 
                                     0);
    if (value)
      oe.file_name_ = value;

    // 4. single file size
    value = this->get_config_attribe(section,
                                     LOG_OUTPUT_SINGLE_FILE_SIZE, 
                                     0);
    if (value)
    {
      fsize = ::atoi(value);
      if (::strrchr(value, 'k') || ::strrchr(value, 'K'))
        fsize *= 1024;
      else
        fsize *= 1024 * 1024;
    }
    // Default is 1M
    if (fsize <= 0)
      fsize = 1 * 1024 * 1024;
    oe.single_file_size_ = fsize;

    // 5. rolloever index
    value = this->get_config_attribe(section,
                                     LOG_OUTPUT_ROLLOEVER, 
                                     0);
    if (value)
      oe.rolloever_ = ::atoi(value);
    else
      oe.rolloever_ = 0;

    // 6. generate file period
    value = this->get_config_attribe(section,
                                     LOG_OUTPUT_GENERATE_FILE_PERIOD, 
                                     0);
    if (value)
      oe.generate_file_period_ = ::atoi(value);
    else
      oe.generate_file_period_ = 0;

OPEN:
    log_output *lo = 0;
    log_manager::output_list_iter p = this->output_list_.find(*iter);
    if (p != this->output_list_.end())
    {
      p->second->update_config(&oe);
      lo = p->second;
    }else
    {
      if (oe.output_type_ == output_entry::om_files)
        lo = new log_to_file();
      else if (oe.output_type_ == output_entry::om_stdout)
        lo = new log_to_stdout();
      if (lo && lo->open(&oe) == 0)
      {
        this->output_list_.insert(std::make_pair(*iter, lo));
      }else
        lo = 0;
    }
    if (lo)
    {
      output_objs.push_back(lo);
    }
  }
  return output_objs;
}
int log_manager::get_log_type(const char *mname)
{
  char *log_type = this->get_config_attribe(MODULES_SECTION,
                                            mname, 
                                            LOG_ATTR_TYPE); 
  int type = 0;
  if (log_type)
  {
    std::deque<std::string> tokens;
    log_help_::get_token_list(log_type, '|', tokens);
    std::deque<std::string>::iterator iter = tokens.begin();
    for (; iter != tokens.end(); ++iter)
    {
      for (size_t j = 0;
           j < sizeof(log_type_list) / sizeof(log_type_list[0]);
           ++j)
      {
        if (::strcmp(iter->c_str(), log_type_list[j]) == 0)
          type |= (1L << j);
      }
    }
    if (type & LL_ALLS)
      type |= LL_ALL_TYPE;
  }
  return type;
}
int log_manager::get_parent_log_type(const char *mname, char *parent)
{
  if (mname == 0) return 0;
  char *pos = ::strrchr((char *)mname, '.');
  int l = 0;
  l = this->get_log_type(mname);
  if (l == -1) l = 0;
  char s[MAX_MODULE_NAME_LEN + 1] = {0};
  if (pos) // grade
  {
    ::strncpy(parent, mname, pos - mname);
#ifdef NDK_LOG_DEBUG
    fprintf(stderr, "log type parent = %s\n", parent);
#endif
    return l | this->get_parent_log_type(parent, s);
  }
  return l;
}
std::deque<std::string> log_manager::get_log_output(const char *mname)
{
  char *output = this->get_config_attribe(MODULES_SECTION,
                                          mname, 
                                          LOG_ATTR_OUTPUT);
  std::deque<std::string> ret;
  if (output)
  {
    log_help_::get_token_list(output, ',', ret);
  }
  return ret;
}
std::deque<std::string> log_manager::get_parent_output(const char *mname) 
{
  char *pos = 0;
  char name[MAX_MODULE_NAME_LEN + 1] = {0};
  std::deque<std::string> ot;
  ::strncpy(name, mname, MAX_MODULE_NAME_LEN);
  do
  {
    pos = ::strrchr(name, '.');
    if (pos) // 
    {
      *pos = '\0';
      std::deque<std::string> ret = this->get_log_output(name);
      if (!ret.empty())
        std::copy(ret.begin(), ret.end(), std::back_inserter(ot));
    }else
      break;
  }while (1);

  return ot;
}
char *log_manager::get_log_max_len_of_one_record(const char *mname)
{
  return this->get_config_attribe(MODULES_SECTION,
                                  mname, 
                                  LOG_ATTR_MAX_LEN_OF_ONE_RECORD); 
}
char *log_manager::get_parent_max_len_of_one_record(const char *mname)
{
  return this->get_parent_value(mname, &log_manager::get_log_max_len_of_one_record);
}
char *log_manager::get_parent_value(const char *mname,
                                    char *(log_manager::*func)(const char *))
{
  char *pos = 0;
  char *value = 0;
  char name[MAX_MODULE_NAME_LEN + 1] = {0};
  ::strncpy(name, mname, MAX_MODULE_NAME_LEN);
  do
  {
    pos = ::strrchr(name, '.');
    if (pos) // grade
    {
      *pos = '\0';
      value = (this->*func)(name);
      if (value) break;
    }else
      break;
  }while (value == 0);

  return value;
}

char *log_manager::get_config_attribe(const char *section, 
                                      const char *key,
                                      const char *attr)
{
  char name[MAX_MODULE_NAME_LEN + 1] = {0};
  if (attr)
    ::snprintf(name, MAX_MODULE_NAME_LEN,
               "%s:%s.%s",
               section,
               key,
               attr);
  else
    ::snprintf(name, MAX_MODULE_NAME_LEN,
               "%s:%s",
               section,
               key);
  char *value = iniparser_getstring(this->ini_obj_, 
                                    name,
                                    0);
  if (value && ::strlen(value) > 0)
  {
    return value;
  }
  return 0;
} 
logger *log_manager::get_logger(const char *module_name)
{
  // Lock here
  guard<thread_mutex> guard(this->log_mutex_);
  return this->get_logger_i(module_name);
}
logger *log_manager::get_logger_i(const char *module_name)
{
  if (module_name == 0 || ::strlen(module_name) == 0)
    return 0;

  // Find exist logger first.
  logger *itor = this->root_node_;
  while (itor)
  {
    if (::strcmp(itor->module_name(), module_name) == 0)
    {
      return itor;
    }
    itor = itor->next();
  }

  logger *l = new logger(this, module_name);
  if (l == 0) return 0;
  module_entry *entry = this->get_module_cfg_entry(module_name);
  l->apply_module_config(entry);
  this->insert_logger(l);
  return l;
}
int log_manager::insert_logger(logger *l)
{
  if (l == 0) return -1;
  l->next(0);
  logger *itor = this->root_node_;
  if (itor == 0)
  {
    this->root_node_ = l;
    return 0;
  }
  while (itor->next()) itor = itor->next();

  itor->next(l);
  return 0;
}
int log_manager::log(logger *log, const char *record, const int len)
{
  // Lock from here !!
  guard<thread_mutex> guard(this->log_mutex_);
  this->current_time_.update();
  this->update_config(this->current_time_.sec());

  return log->output(record, len, 0, this->current_time_.sec());
}
int log_manager::log(logger *log, 
                     int lt, 
                     const char *format,
                     va_list &va_ptr)
{
  // Lock from here !!
  guard<thread_mutex> guard(this->log_mutex_);
  this->current_time_.update();
  time_t now = this->current_time_.sec();

  this->update_config(now);

  if (log->shutdown() || !log->enable_for(lt))
    return 0;

  // Format
  this->current_dtime_.update(this->current_time_.sec());
  this->current_dtime_.to_str(this->log_time_, 
                              sizeof(this->log_time_));
  this->current_time_.sec(0);

  int len = 0;
  int max_len_of_one_record = log->module_entry_->max_len_of_one_record_;;
  int ret = ::snprintf(this->log_record_, 
                       max_len_of_one_record, 
                       "[%s.%03lu][%lu]<%s><%s>: ",

                       this->log_time_,
                       this->current_time_.msec(),
                       (unsigned long int)pthread_self(),
                       log_types[(log_help_::get_bit(lt) - 1) % log_types_size],
                       log->module_name());
  if (ret > max_len_of_one_record)
    ret = max_len_of_one_record - 1;
  len += ret;
  if (len < max_len_of_one_record)
  {
    ret = ::vsnprintf(this->log_record_ + len, 
                      max_len_of_one_record - len,
                      format,
                      va_ptr);
    if (ret < 0) return -1;
    /**
     * check overflow or not
     * Note : snprintf and vnprintf return value is the number of characters
     *(not including the trailing ’\0’) which would have been  written  to 
     * the  final  string  if enough space had been available.
     */
    if (ret > (max_len_of_one_record - len))
      ret = max_len_of_one_record - len - 1;
    // vsnprintf return the length of <va_ptr> actual
    len += ret;
  }
  this->log_record_[len] = '\n';
  this->log_record_[len + 1] = '\0';
  //
  return log->output(this->log_record_, len + 1, lt, now);
}
void log_manager::dump_cfg(void)
{
#if 0
  logger *l = this->root_node_;
  while (l)
  {
    fprintf(stderr, "%s ================\n", l->module_name_);
    fprintf(stderr, "  log_type = %d\n", l->module_entry_->log_type_);
    fprintf(stderr, "  output = %s,", l->module_entry_->output_list_[0].c_str());
    for (size_t i = 1; i < l->module_entry_->output_list_.size(); ++i)
      fprintf(stderr, "%s,", l->module_entry_->output_list_[i].c_str());
    fprintf(stderr, "\n");
    fprintf(stderr, "  max_len_of_one_record = %d\n", l->module_entry_->max_len_of_one_record_);
    l = l->next();
  }
#endif
}
} // namespace ndk

