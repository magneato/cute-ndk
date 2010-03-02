#include "ndk/strace.h"

namespace ndk
{
#ifdef NDK_STRACE
size_t strace::count_ = 0;
thread_mutex strace::mutex_;
#endif
} // namespace ndk

