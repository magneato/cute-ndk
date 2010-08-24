#include "reactor_event_handler.h"
#include "push_sessionmgr.h"

#include <ndk/logger.h>

static ndk::logger *event_log = ndk::log_manager::instance()->get_logger("root.event");

int reactor_event_handler::handle_msg(void *msg)
{
  notify_event *event = reinterpret_cast<notify_event *>(msg);

  assert(event != 0);

  switch(event->message_id)
  {
  case 1000:
    //handle_pull_file_ok(event);
    break;
  default:
    push_session_ptr push_ss = push_sessionmgr::instance()->find(event->session_id);
    if (push_ss)
    {
      push_ss->client()->close();
    }else
    {
      event_log->debug("reactor event handler not find session: %d",
                       event->session_id);
    }
  }
  delete event;

  return 0;
}
