// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-20 19:05
 */
//========================================================================

#ifndef NDK_LOG_OUTPUT_H_
#define NDK_LOG_OUTPUT_H_

#include <string>

namespace ndk
{
  /**
   * @class output_entry
   *
   */
  class output_entry
  {
  public:
    enum
    {
      om_shutdown     = 1L << 0,
      om_stdout       = 1L << 1,
      om_files        = 1L << 2
    };

    output_entry()
      : output_type_(om_shutdown),
      single_file_size_(0),
      rolloever_(0),
      generate_file_period_(0)
    { }

    ~output_entry()
    { }

    output_entry &operator = (const output_entry &ce)
    {
      if (&ce == this) return *this;
      this->output_type_           = ce.output_type_;
      this->single_file_size_      = ce.single_file_size_;
      this->rolloever_             = ce.rolloever_;
      this->generate_file_period_  = ce.generate_file_period_;
      this->file_name_             = ce.file_name_;
      this->log_dir_               = ce.log_dir_;
      return *this;
    }
    inline bool operator == (const output_entry &ce)
    {
      if (&ce == this) return true;
      if ((this->log_dir_ == ce.log_dir_)
          && (this->file_name_ == ce.file_name_))
        return true;
      return false;
    }

    inline bool operator != (const output_entry &ce)
    {
      return !(*this == ce);
    }

  public:
    int output_type_;

    int single_file_size_;

    int rolloever_;

    int generate_file_period_;

    std::string file_name_;

    std::string log_dir_;
  };
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
    log_output()
      : type_(output_entry::om_shutdown)
    { }

    // No-op virtual destructor.
    virtual ~log_output() { }

    /**
     * Open the back end object. Perform any actions needed to prepare
     * the object for later logging operations.
     *
     * @retval 0 for success.
     * @retval -1 for failure.
     */
    virtual int open(const output_entry *) = 0;

    // Close the output completely.
    virtual int close(void) = 0;

    // Reset the strategy.
    virtual int reset(void) = 0;

    // Update log strategy
    virtual void update_config(const output_entry *) = 0;

    /**
     * Process a log record.
     *
     * @retval -1 for failure; else it is customarily the number of bytes
     * processed, on zero is a no-op.
     */
    virtual int log(const char *log_msg, int len, int ll, time_t now = 0) = 0;

    //
    int type(void)const { return this->type_; }
  protected:
    int type_;
  };
} // namespace ndk

#endif // NDK_LOG_OUTPUT_H_

