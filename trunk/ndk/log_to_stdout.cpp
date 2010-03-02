#include "log_to_stdout.h"
#include "logger.h"

namespace ndk
{

log_to_stdout::log_to_stdout()
{
    type_ = output_entry::om_stdout;
}

log_to_stdout::~log_to_stdout()
{
}
int log_to_stdout::open(const output_entry *)
{
  setbuf(stdout, 0);
  return 0;
}
int log_to_stdout::close()
{
  return 0;
}
int log_to_stdout::reset()
{
  return 0;
}
int log_to_stdout::log(const char *log_msg, int, int ll, time_t )
{
  if (ll & LL_DEBUG)
    return ::fprintf(stdout, "\033[0;33m%s\033[0m", log_msg);
  else if (ll & LL_TRACE)
    return ::fprintf(stdout, "%s", log_msg);
  else if (ll & LL_RINFO)
    return ::fprintf(stdout, "\033[0;32m%s\033[0m", log_msg);
  else if (ll & LL_ERROR)
    return ::fprintf(stdout, "\033[1;31m%s\033[0m", log_msg);
  else if (ll & LL_WNING)
    return ::fprintf(stdout, "\033[0;35m%s\033[0m", log_msg);
  else if (ll & LL_FATAL)
    return ::fprintf(stdout, "\033[4;36m%s\033[0m", log_msg);
  return ::fprintf(stdout, "%s", log_msg);
}
void log_to_stdout::update_config(const output_entry *)
{
}
} // namespace ndk

