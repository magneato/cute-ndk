// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-20 19:04
 */
//========================================================================

#ifndef NDK_NDK_LOG_H_
#define NDK_NDK_LOG_H_

#include "ndk/config.h"
#include "ndk/log_manager.h"
#include "ndk/global_macros.h"

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

  LL_ALLS     = LL_TRACE \
                | LL_DEBUG 
                | LL_WNING 
                | LL_ERROR 
                | LL_RINFO 
                | LL_FATAL
};
// C++ standard stream output format.
#define STD_OUTPUT_FMT(lg, ll, record)			    \
  do{							                              \
    std::ostringstream ostr__;			            \
    ostr__ << record;				                    \
    lg->log (ll, "%s", ostr__.str().c_str());		\
  }while(0)

#define LOG_TRACE(log, record) STD_OUTPUT_FMT(log, LL_TRACE, record)				

#define LOG_DEBUG(log, record) STD_OUTPUT_FMT(log, LL_DEBUG, record)				

#define LOG_WNING(log, record) STD_OUTPUT_FMT(log, LL_WNING, record)				

#define LOG_ERROR(log, record) STD_OUTPUT_FMT(log, LL_ERROR, record)				

#define LOG_RINFO(log, record) STD_OUTPUT_FMT(log, LL_RINFO, record)				

#define LOG_FATAL(log, record) STD_OUTPUT_FMT(log, LL_FATAL, record)				

#define METHOD_TRACE(L) mtrace ____ (L,                   \
                                     __FILE__,            \
                                     __PRETTY_FUNCTION__, \
                                     __LINE__)
namespace ndk
{
  /**
   * @class ndk_log
   * 
   * @brief 
   */
  class ndk_log
  {
    friend class log_manager;
  protected:
    // Constructor
    ndk_log(log_manager *lm, const char *module_name)
      : log_level_(0),
      parent_llevel_(0),
      module_name_(0),
      next_node_(0),
      log_manager_(lm)
    { 
      int len = ::strlen(module_name);
      module_name_ = new char[MAX_MODULE_NAME_LEN + 1];
      ::memset(module_name_, '\0', MAX_MODULE_NAME_LEN + 1);
      ::strncpy(module_name_, module_name, len);
    }

    //
    ~ndk_log()
    {
      if (this->module_name_)
        delete []this->module_name_;
      this->module_name_ = 0;
    }

  public:
    // Without any format to be output.
    inline int puts(const char *record, const int len)
    { return this->log_manager_->log(this, record, len); }

    inline int log(int ll, const char *format, ...)
    {
      /**
       * For optimising, we can call 
       * if (log->shutdown () || 
       * 	  !log->enable_for (ll))
       * return 0;
       * here, but can't call LogManager::update_config(), can't
       * load the new config in runing time.
       */
      //
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
        NDK_SET_BITS(this->log_level_, LL_SHUTDOWN);
      else
        NDK_CLR_BITS(this->log_level_, LL_SHUTDOWN);
    }
  protected:
    //
    bool shutdown(void)
    {
      return NDK_BIT_ENABLED(this->log_level_, LL_SHUTDOWN) ||
        NDK_BIT_ENABLED(this->parent_llevel_, LL_SHUTDOWN);
    }

    //
    inline bool enable_for(int ll)
    {
      return NDK_BIT_ENABLED (this->log_level_, ll) || 
        NDK_BIT_ENABLED (this->parent_llevel_, ll);
    }

    // Get log level.
    inline int log_level(void)          { return this->log_level_;      }

    // Set log level.
    inline void log_level(int lv)       { this->log_level_ = lv;        }

    // Get parent's log level
    inline int parent_llevel(void)      { return this->parent_llevel_;  }

    // Set parent's log level
    inline void parent_llevel(int lv)   { this->parent_llevel_ = lv;    }

    // = For list struct
    // Get next node.
    inline ndk_log *next(void)          { return this->next_node_;      }

    // Set next node.
    inline void next(ndk_log *nl)       { this->next_node_ = nl;        }

    //
    inline const char *module_name(void){ return this->module_name_;   }

    // 
    inline void module_name(const char *name) 
    { 
      if (name == 0) return ;
      ::strncpy(this->module_name_, name, MAX_MODULE_NAME_LEN);
    }
  protected:
    void applay_config();
  private:
    // 
    int log_level_;

    int parent_llevel_;

    char *module_name_;

    ndk_log *next_node_;

    log_manager *log_manager_;
  }; // class ndk_log

  /**
   * @class mtrace
   *
   * @brief 
   */
  class mtrace
  {
  public:
    inline mtrace(ndk_log *log, const char *file, 
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
    ndk_log	*     log_;
    char          spaces_[128];
    static size_t count_;
  };
} // namespace ndk

#endif // NDK_NDK_LOG_H_

