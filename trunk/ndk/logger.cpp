#include "logger.h"
#include "log_to_file.h"
#include "log_to_stdout.h"

namespace ndk 
{

int logger::apply_module_config(module_entry *entry)
{
  if (entry == 0) return -1;
  if (this->module_entry_ != 0)
  {
    delete this->module_entry_;
  }
  this->module_entry_ = entry;
  return this->apply_module_config_i();
}
int logger::apply_module_config_i()
{
  this->output_list_ = this->log_manager_->get_output_list(this->module_entry_);
  return 0;
}
int logger::output(const char *record, const int len, int log_type, time_t now)
{
  if (this->output_list_.empty()) return 0;
  std::deque<log_output *>::iterator iter = this->output_list_.begin();
  int result = 0;
  for (; iter != this->output_list_.end(); ++iter)
    result = (*iter)->log(record, len, log_type, now);
  return result;
}

size_t mtrace::count_ = 0;
} // namespace ndk

