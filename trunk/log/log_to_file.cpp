#include "ndk/log_to_file.h"
#include "ndk/ndk_os.h"

namespace ndk
{
log_to_file::log_to_file()
  : log_handle_(NDK_INVALID_HANDLE),
  current_file_size_(0),
  file_name_(0),
  log_dir_(0),
  config_entry_(0),
  old_config_entry_(0)
{
  file_name_ = new char[NDK_MAX_FILENAME_LEN + 1];
  log_dir_   = new char[NDK_MAX_FILENAME_LEN + 1];
  ::memset(file_name_, '\0', NDK_MAX_FILENAME_LEN + 1);
#ifdef NDK_LOG_DEBUG
  fprintf(stderr, "Construct log_to_file\n");
#endif
}
log_to_file::~log_to_file()
{
  if (this->file_name_)
    delete []this->file_name_;
  this->file_name_ = 0;

  if (this->log_dir_)
    delete []this->log_dir_;
  this->log_dir_ = 0;

  if (this->config_entry_)
    delete this->config_entry_;
  this->config_entry_ = 0;

  this->close();
}
int log_to_file::open(config_entry *cfg)
{
  if (cfg == 0) return -1;

  // opened.
  if (this->config_entry_) return -1;

  // Save strategy.
  this->config_entry_ = new config_entry;
  *this->config_entry_ = *cfg;

  this->last_calc_dtime_.update();
  // Build file name
  this->build_filename();

  // Precalculate the time of the next file.
  this->calc_next_time();

  return this->create_file();
}
int log_to_file::create_file(void)
{
  // Check and create dir
  if (ndk_os::mkdir(this->config_entry_->log_dir_, 0755) == -1)
  {
    if (errno != EEXIST) return -1;
    errno = 0;
  }
  // Get file size. If the file is exsit and its size is out of 
  // configure, then create a new one
  this->close();
  ndk_stat bf;
  ::memset(&bf, 0, sizeof(bf));
  if (ndk_os::stat(this->file_name_, &bf) == 0)
  {
    this->current_file_size_ = bf.st_size;
    this->check_file_size();
  }
  // Open file
  this->log_handle_ 
    = ndk_os::open(this->file_name_, O_CREAT | O_RDWR | O_APPEND, 0644);
  if (this->log_handle_ == NDK_INVALID_HANDLE) // create file failed
  {
    NDK_LOG("Open file failed : %s", strerror(ndk_os::last_error()));
    return -1;
  }
  return 0;
}
int log_to_file::close(void)
{
  if (this->log_handle_ != NDK_INVALID_HANDLE)
    ndk_os::close(this->log_handle_);
  this->log_handle_ = NDK_INVALID_HANDLE;
  return 0;
}
void log_to_file::reset(void)
{
  // Build file name
  this->build_filename();

  // Precalculate the time of the next file.
  this->calc_next_time();

  this->close();

  this->create_file();
}
void log_to_file::update_config(const config_entry *entry)
{
  if (this->old_config_entry_ == 0)
    this->old_config_entry_ = new config_entry;
  *this->old_config_entry_ = *this->config_entry_;
  *this->config_entry_ = *entry;
  if ((this->config_entry_->generate_file_period_
       != this->old_config_entry_->generate_file_period_)
      ||
      (::strncmp(this->config_entry_->file_name_, 
                 this->old_config_entry_->file_name_,
                 NDK_MAX_FILENAME_LEN) != 0)
      ||
      (::strncmp(this->config_entry_->log_dir_,
                 this->old_config_entry_->log_dir,
                 NDK_MAX_FILENAME_LEN) != 0)
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
    this->log_handle_ 
      = ndk_os::open(this->file_name_, O_CREAT | O_RDWR | O_APPEND, 0644);
  }else if (this->config_entry_->generate_file_period_ > 0)
  // 2. Check a_file_a_period is on or not
  {
    // Check time
    this->generate_period_file(now);
  }

  // 3. Record the log msg
  int result = 0;
  if ((result = ndk_os::write(this->log_handle_, log_msg, len)) <= 0)
  {
    this->close();
    return -1;
  }
  this->current_file_size_ += result;
  return result;
}
int log_to_file::check_file_size(void)
{
  if (this->current_file_size_ >= 
      this->config_entry_->single_file_size_)
  {
    this->close();

    // Backup file, switch file
    if (this->config_entry_->max_rolloever_index_ > 0)
    {
      this->rolloever_files();
    }else
    {
      ndk_os::unlink(this->file_name_);
    }
    this->current_file_size_ = 0;
    return 1;
  }
  return 0;
}
int log_to_file::rolloever_files()
{
  char src[NDK_MAX_FILENAME_LEN + 1] = {0};
  char tgt[NDK_MAX_FILENAME_LEN + 1] = {0};
  for (int i = this->config_entry_->max_rolloever_index_ - 1; 
       i >= 1; 
       --i)
  {
    ndk_os::snprintf(src, NDK_MAX_FILENAME_LEN, 
                     "%s.%d",
                     this->file_name_,
                     i);
    ndk_os::snprintf(tgt, NDK_MAX_FILENAME_LEN, 
                     "%s.%d",
                     this->file_name_,
                     i + 1);
    ndk_os::rename(src, tgt);
  }
  ::memset(tgt, 0, sizeof(NDK_MAX_FILENAME_LEN));
  ndk_os::snprintf(tgt, NDK_MAX_FILENAME_LEN, 
                   "%s.%d",
                   this->file_name_,
                   1);
  ndk_os::rename(this->file_name_, tgt);
  return 0;
}
void log_to_file::build_filename()
{
  ::memset(this->file_name_, '\0', NDK_MAX_FILENAME_LEN);
  ::strncat(this->file_name_, 
            this->config_entry_->log_dir_,
            NDK_MAX_FILENAME_LEN);
  ::strncat(this->file_name_, "/", 1);
  ::strncat(this->file_name_, 
           this->config_entry_->file_name_, 
           NDK_MAX_FILENAME_LEN);
}
void log_to_file::generate_period_file(time_t now)
{
  if (now == 0) now = ::time(0);
  if (difftime(now, this->last_generate_period_file_time_) >= 0)   // next period
  {
    this->close();
    this->rolloever_files();
    this->build_filename();
    this->log_handle_ handle = ndk_os::open(this->file_name_, 
                                            O_CREAT | O_RDWR | O_APPEND, 0644);
    this->last_generate_period_file_time_ = now;
#ifdef NDK_LOG_DEBUG
    fprintf(stderr, "Open file over time %lu - %lu : %s\n", 
            now, 
            this->last_generate_new_file_time_,
            this->file_name_);
#endif
    if (this->log_handle_ == NDK_INVALID_HANDLE) // create file failed
    {
      NDK_LOG("Open file error : %s", strerror(ndk_os::last_error()));
    }
  }
}
} // namespace ndk

