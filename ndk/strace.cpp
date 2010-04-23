#include "ndk/strace.h"

namespace ndk
{
size_t strace::count_ = 0;
thread_mutex strace::mutex_;
} // namespace ndk

