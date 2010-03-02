// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-12-19 16:24
 */
//========================================================================

#ifndef NDK_LOGGER_H_
#define NDK_LOGGER_H_

#include "ndk/log_output.h"
#include "ndk/log_manager.h"

#include <cstring>
#include <sstream>
#include <stdarg.h>

enum 
{
  // = Note, this first argument *must* start at 1!

  // Shutdown the logger
  LL_SHUTDOWN = 1L << 0,
  LL_TRACE	  = 1L << 1,	
  LL_DEBUG	  = 1L << 2,
  LL_WNING	  = 1L << 3,
  LL_ERROR    = 1L << 4,
  LL_RINFO	  = 1L << 5,
  LL_FATAL	  = 1L << 6,
  LL_ALLS     = 1L << 7,

  LL_ALL_TYPE = LL_TRACE \
                | LL_DEBUG 
                | LL_WNING 
                | LL_ERROR 
                | LL_RINFO 
                | LL_FATAL
};
// C++ standard stream output format.
#define STD_OUTPUT_FMT(lg, ll, record)                          \
  do{                                                           \
    std::ostringstream os;                                      \
    os<< record;                                                \
    lg->log(ll, "%s", os.str().c_str());                        \
  }while(0)

#define LOG_TRACE(log, record) STD_OUTPUT_FMT(log, LL_TRACE, record)				

#define LOG_DEBUG(log, record) STD_OUTPUT_FMT(log, LL_DEBUG, record)				

#define LOG_WNING(log, record) STD_OUTPUT_FMT(log, LL_WNING, record)				

#define LOG_ERROR(log, record) STD_OUTPUT_FMT(log, LL_ERROR, record)				

#define LOG_RINFO(log, record) STD_OUTPUT_FMT(log, LL_RINFO, record)				

#define LOG_FATAL(log, record) STD_OUTPUT_FMT(log, LL_FATAL, record)				

#define LOG_PUTS(log, record)                                   \
  do{                                                           \
    std::ostringstream os;                                      \
    os << record;                                               \
    lg->puts(os.str().c_str(), os.str().length());              \
  }while(0)

#define METHOD_TRACE(L) ndk::mtrace o_o(L,                      \
                                        __FILE__,               \
                                        __PRETTY_FUNCTION__,    \
                                        __LINE__)
namespace ndk 
{
  /**
   * @class logger
   * 
   * @brief 
   */
  class logger
  {
    friend class log_manager;
  protected:
    // Constructor
    logger(log_manager *lm, const char *module_name)
      : log_type_(0),
      module_name_(0),
      module_entry_(0),
      next_node_(0),
      log_manager_(lm)
    { 
      module_name_ = new char[MAX_MODULE_NAME_LEN + 1];
      ::memset(module_name_, '\0', MAX_MODULE_NAME_LEN + 1);
      ::strncpy(module_name_, module_name, MAX_MODULE_NAME_LEN);
    }

    //
    ~logger()
    {
      if (this->module_name_)
        delete []this->module_name_;
      this->module_name_ = 0;

      if (this->module_entry_ == 0)
        delete this->module_entry_;
      this->module_entry_ = 0;
    }

  public:
    // Without any format to be output.
    inline int puts(const char *record, const int len)
    { 
      return this->log_manager_->log(this, record, len); 
    }

    inline int log(int ll, const char *format, ...)
    {
      va_list va;
      va_start(va, format);
      int ret = this->log_manager_->log(this, ll, format, va);
      va_end (va);
      return ret;
    }

    // == Special log method
# define SHORT_CODE(LL) va_list va;                          \
    va_start(va, format);                                    \
    int ret = this->log_manager_->log(this, LL, format, va); \
    va_end(va);                                              \
    return ret

    inline int trace(const char *format, ...)    { SHORT_CODE(LL_TRACE); }

    inline int debug(const char *format, ...)    { SHORT_CODE(LL_DEBUG); }

    inline int warning(const char *format, ...)  { SHORT_CODE(LL_WNING); }

    inline int error(const char *format, ...)    { SHORT_CODE(LL_ERROR); }

    inline int rinfo(const char *format, ...)    { SHORT_CODE(LL_RINFO); }

    inline int fatal(const char *format, ...)    { SHORT_CODE(LL_FATAL); }

#undef SHORT_CODE

    /**
     * This method just close ensure logger only, do not close 
     * its child's logger.
     * @param switch_val : true is shutdown
     * 			   false is reopen
     */
    inline void shutdown(bool switch_val)
    {
      if (switch_val) // shutdown
        this->module_entry_->log_type_ |= LL_SHUTDOWN;
      else
        this->module_entry_->log_type_ &= ~LL_SHUTDOWN;
    }
  protected:
    //
    inline bool shutdown(void)
    {
      return this->module_entry_->log_type_ & LL_SHUTDOWN;
    }

    //
    inline bool enable_for(int lt)
    {
      return this->module_entry_->log_type_ & lt;
    }

    // Get log type.
    inline int log_type(void)          { return this->log_type_;      }

    // Set log type.
    inline void log_type(int lt)       { this->log_type_ = lt;        }

    // = For list struct
    // Get next node.
    inline logger *next(void)          { return this->next_node_;      }

    // Set next node.
    inline void next(logger *nl)       { this->next_node_ = nl;        }

    //
    inline const char *module_name(void){ return this->module_name_;   }

    // 
    inline void module_name(const char *name) 
    { 
      if (name == 0) return ;
      ::strncpy(this->module_name_, name, MAX_MODULE_NAME_LEN);
    }

    int output(const char *, const int len, int log_type, time_t now);

    //
    int apply_module_config(module_entry *);

    int apply_module_config_i();

  private:
    // 
    int log_type_;

    char *module_name_;

    module_entry *module_entry_;

    logger *next_node_;

    std::deque<log_output *> output_list_;

    log_manager *log_manager_;
  }; // class logger

  /**
   * @class mtrace
   *
   * @brief 
   */
  class mtrace
  {
  public:
    inline mtrace(logger *log, const char *file, 
                  const char *func, 
                  int line)
      : line_(line),
      file_(file),
      func_(func),
      log_(log)
    {
      if (mtrace::count_ >= sizeof(this->spaces_) / 2) --mtrace::count_;
      std::memset(this->spaces_, ' ', mtrace::count_ * 2);
      *(this->spaces_ + mtrace::count_ * 2) = '\0';
      this->log_->trace("%s(%d)calling `%s` in file `%s` on line `%d`",
                        this->spaces_, mtrace::count_++, 
                        func_, file_, line_);
    }

    inline ~mtrace()
    {
      if (mtrace::count_ >= sizeof(this->spaces_) / 2) --mtrace::count_;
      std::memset(this->spaces_, ' ', (mtrace::count_ - 1) * 2);
      *(this->spaces_ + (mtrace::count_ - 1) * 2) = '\0';
      this->log_->trace("%s(%d)leaving `%s`", this->spaces_, 
                        --mtrace::count_, func_);
    }
  private:
    int           line_;
    const  char * file_;
    const  char * func_;
    logger	*     log_;
    char          spaces_[128];
    static size_t count_;
  };
} // namespace ndk

#endif // NDK_LOGGER_H_

