#include "ndk/strace.h"

namespace ndk
{
unsigned int strace::count_ = 0;
thread_mutex strace::mutex_;
} // namespace ndk

