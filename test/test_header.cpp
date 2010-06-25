#include "ndk/acceptor.h"
#include "ndk/circular_buffer.h"
#include "ndk/condition.h"
#include "ndk/config-base.h"
#include "ndk/config.h"
#include "ndk/config-linux.h"
#include "ndk/connector.h"
#include "ndk/date_time.h"
#include "ndk/dictionary.h"
#include "ndk/epoll_reactor.h"
#include "ndk/event_handler.h"
#include "ndk/global_macros.h"
#include "ndk/guard.h"
#include "ndk/inet_addr.h"
#include "ndk/iniparser.h"
#include "ndk/logger.h"
#include "ndk/log_manager.h"
#include "ndk/log_output.h"
#include "ndk/log_to_file.h"
#include "ndk/log_to_stdout.h"
#include "ndk/mem_pool.h"
#include "ndk/message_block.h"
#include "ndk/message_queue.h"
#include "ndk/ndk.h"
#include "ndk/pipe.h"
#include "ndk/reactor.h"
#include "ndk/reactor_impl.h"
#include "ndk/reactor_token.h"
#include "ndk/recursive_thread_mutex.h"
#include "ndk/rw_thread_mutex.h"
#include "ndk/select_reactor.h"
#include "ndk/semaphore.h"
#include "ndk/singleton.h"
#include "ndk/sock_acceptor.h"
#include "ndk/sock_connector.h"
#include "ndk/sock_dgram.h"
#include "ndk/socket.h"
#include "ndk/sock_stream.h"
#include "ndk/strace.h"
#include "ndk/svc_handler.h"
#include "ndk/task_base.h"
#include "ndk/task.h"
#include "ndk/thread.h"
#include "ndk/thread_manager.h"
#include "ndk/thread_mutex.h"
#include "ndk/timer_queue.h"
#include "ndk/time_value.h"
#include "ndk/token.h"
#include "ndk/types.h"
#include "ndk/unix_reactor.h"
#include "ndk/unused_arg.h"

// ls -l ndk/*.h | awk '{print "#include", "\"" $NF "\"";}'
int main ()
{
  return 0;
}