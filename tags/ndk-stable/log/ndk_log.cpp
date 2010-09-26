#include "ndk/ndk_log.h"

namespace ndk
{

void ndk_log::apply_module_config(config_entry *entry)
{
  if (entry == 0) return ;
  if (this->config_entry_ == 0)
  {
    this->config_entry_ = entry;
  }else if (*entry != *this->config_entry_)
  {
    this->config_entry_ = entry;
    return this->apply_module_config_i();
  }else
  {
    delete entry;
  }
}
int ndk_log::apply_module_config_i()
{
  // 1. Ouput module
  if (NDK_BIT_ENABLED(this->config_entry_->output_, 
                      log_manager::om_files))
  {
    if (this->files_output_ == 0)
    {
      this->files_output_ = new log_to_file();
      this->files_output_->open(this->config_entry_);
    }else
    {
      this->files_output_->update_config(this->config_entry_);
    }
  }else
  {
    if (this->files_output_)
    {
      delete this->files_output_;
      this->files_output_ = 0;
    }
  }
  //  
  if (NDK_BIT_ENABLED(this->config_entry_->output_, 
                      log_manager::om_stdout))
  {
    if (this->stdout_output_ == 0)
    {
      this->stdout_output_ = new log_to_stdout();
      this->stdout_output_->open(this->config_entry_);
    }else
    {
      this->stdout_output_->update_config(this->config_entry_);
    }
  }else
  {
    if (this->stdout_output_)
    {
      delete this->stdout_output_;
      this->stdout_output_ = 0;
    }
  }
  return 0;
}
size_t mtrace::count_ = 0;
} // namespace ndk

