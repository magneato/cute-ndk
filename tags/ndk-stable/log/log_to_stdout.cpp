#include "ndk/log_to_stdout.h"
#include "ndk/ndk_log.h"

namespace ndk
{

log_to_stdout::log_to_stdout()
{
}

log_to_stdout::~log_to_stdout()
{
}
int log_to_stdout::open(const char *, log_manager::strategy *)
{
  setbuf(stdout, 0);
  return 0;
}
int log_to_stdout::close()
{
  return 0;
}
void log_to_stdout::reset()
{
}
int log_to_stdout::log(const char *log_msg, int, int ll)
{
#if defined(NDK_LINUX)
  if (NDK_BIT_ENABLED(ll, LL_DEBUG))
    return ::fprintf(stdout, "\e[0;33m%s\e[0m", log_msg);
  else if (NDK_BIT_ENABLED(ll, LL_TRACE))
    return ::fprintf(stdout, "%s", log_msg);
  else if (NDK_BIT_ENABLED(ll, LL_RINFO))
    return ::fprintf(stdout, "\e[0;32m%s\e[0m", log_msg);
  else if (NDK_BIT_ENABLED(ll, LL_ERROR))
    return ::fprintf(stdout, "\e[0;31m%s\e[0m", log_msg);
  else if (NDK_BIT_ENABLED(ll, LL_WNING))
    return ::fprintf(stdout, "\e[0;35m%s\e[0m", log_msg);
  else if (NDK_BIT_ENABLED(ll, LL_FATAL))
    return ::fprintf(stdout, "\e[0;36m%s\e[0m", log_msg);
#endif
  return ::fprintf(stdout, "%s", log_msg);
}
void log_to_stdout::update_strategy(const char *, log_manager::strategy *)
{
}
} // namespace ndk

