#include "ndk/log_to_file.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

namespace ndk 
{
log_to_file::log_to_file()
  : log_handle_(NDK_INVALID_HANDLE),
  current_file_size_(0),
  output_entry_(0),
  old_output_entry_(0)
{
  last_generate_period_file_time_ = 0;
  type_ = output_entry::om_files;
}
log_to_file::~log_to_file()
{
  if (this->output_entry_)
    delete this->output_entry_;
  this->output_entry_ = 0;

  if (this->old_output_entry_)
    delete this->old_output_entry_;
  this->old_output_entry_ = 0;

  this->close();
}
int log_to_file::open(const output_entry *cfg)
{
  if (cfg == 0) return -1;

  // opened.
  if (this->output_entry_) return -1;

  // Save strategy.
  this->output_entry_ = new output_entry;
  *this->output_entry_ = *cfg;

  this->last_generate_period_file_time_ = ::time(0);

  return this->reset();
}
int log_to_file::create_file(void)
{
  // Check and create dir
  if (::mkdir(this->output_entry_->log_dir_.c_str(), 0755) == -1)
  {
    if (errno != EEXIST) return -1;
    errno = 0;
  }
  // Get file size. If the file is exsit and its size is out of 
  // configure, then create a new one
  this->close();
  struct stat bf;
  ::memset(&bf, 0, sizeof(bf));
  if (::stat(this->file_name_.c_str(), &bf) == 0)
  {
    this->current_file_size_ = bf.st_size;
    this->check_file_size();
  }
  // Open file
  this->log_handle_ = ::open(this->file_name_.c_str(), 
                             O_CREAT | O_RDWR | O_APPEND, 
                             0644);
  if (this->log_handle_ == NDK_INVALID_HANDLE) // create file failed
  {
    return -1;
  }
  return 0;
}
int log_to_file::close(void)
{
  if (this->log_handle_ != NDK_INVALID_HANDLE)
    ::close(this->log_handle_);
  this->log_handle_ = NDK_INVALID_HANDLE;
  return 0;
}
int log_to_file::reset(void)
{
  // Build file name
  this->build_filename();

  this->close();

  return this->create_file();
}
void log_to_file::update_config(const output_entry *entry)
{
  if (this->old_output_entry_ == 0)
    this->old_output_entry_ = new output_entry;
  *this->old_output_entry_ = *this->output_entry_;
  *this->output_entry_ = *entry;
  if ((this->output_entry_->generate_file_period_
       != this->old_output_entry_->generate_file_period_)
      ||
      (this->output_entry_->file_name_ != this->old_output_entry_->file_name_)
      ||
      (this->output_entry_->log_dir_ != this->old_output_entry_->log_dir_)
     )
  {
    this->reset();
  }
}
int log_to_file::log(const char *log_msg, int len, int , time_t now/* = 0*/)
{
  if (this->log_handle_ == NDK_INVALID_HANDLE)
    return -1;

  // 1. Check file size
  if (this->check_file_size())
  {
    this->close();
    this->log_handle_ = ::open(this->file_name_.c_str(), 
                               O_CREAT | O_RDWR | O_APPEND, 
                               0644);
  }else if (this->output_entry_->generate_file_period_ > 0)
    // 2. Check a_file_a_period is on or not
  {
    // Check time
    if (this->generate_period_file(now) != 0)
      return -1;
  }

  // 3. Record the log msg
  int result = 0;
  do
  {
    if ((result = ::write(this->log_handle_, log_msg, len)) <= 0)
    {
      if (errno == ENOSPC)
        return 0;
      else if (errno == EINTR)
        continue;
      this->close();
      return -1;
    }
  }while (0);
  this->current_file_size_ += result;
  return result;
}
int log_to_file::check_file_size(void)
{
  if (this->current_file_size_ >= 
      this->output_entry_->single_file_size_)
  {
    // Backup file, switch file
    if (this->output_entry_->rolloever_> 0)
    {
      this->rolloever_files();
    }else
    {
      ::unlink(this->file_name_.c_str());
    }
    this->current_file_size_ = 0;
    return 1;
  }
  return 0;
}
int log_to_file::rolloever_files()
{
  char src[FILENAME_MAX + 1] = {0};
  char tgt[FILENAME_MAX + 1] = {0};
  for (int i = this->output_entry_->rolloever_ - 1; 
       i >= 1; 
       --i)
  {
    ::snprintf(src, FILENAME_MAX, 
               "%s.%d",
               this->file_name_.c_str(),
               i);
    ::snprintf(tgt, FILENAME_MAX, 
               "%s.%d",
               this->file_name_.c_str(),
               i + 1);
    ::rename(src, tgt);
    ::memset(src, 0, sizeof(FILENAME_MAX));
    ::memset(tgt, 0, sizeof(FILENAME_MAX));
  }
  ::memset(tgt, 0, sizeof(FILENAME_MAX));
  ::snprintf(tgt, FILENAME_MAX, 
             "%s.%d",
             this->file_name_.c_str(),
             1);
  ::rename(this->file_name_.c_str(), tgt);
  return 0;
}
void log_to_file::build_filename()
{
  this->file_name_.clear();

  this->file_name_ += this->output_entry_->log_dir_ 
    + "/" 
    + this->output_entry_->file_name_;
}
int log_to_file::generate_period_file(time_t now)
{
  if (now == 0) now = ::time(0);
  if (difftime(now, this->last_generate_period_file_time_) 
      >= this->output_entry_->generate_file_period_)   // next period
  {
    this->close();
    if (this->output_entry_->rolloever_> 0)
      this->rolloever_files();
    else
      ::unlink(this->file_name_.c_str());
    this->build_filename();
    this->log_handle_ = ::open(this->file_name_.c_str(), 
                               O_CREAT | O_RDWR | O_APPEND, 
                               0644);
    this->last_generate_period_file_time_ = now;

    if (this->log_handle_ == NDK_INVALID_HANDLE) // create file failed
    {
      return -1;
    }
  }
  return 0;
}
} // namespace ndk

