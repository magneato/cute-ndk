#include "reactor_event_handler.h"
#include "push_sessionmgr.h"
#include "pull_sessionmgr.h"

#include <ndk/logger.h>

static ndk::logger *event_log = ndk::log_manager::instance()->get_logger("root.event");

int reactor_event_handler::handle_msg(void *msg)
{
  notify_event *event = reinterpret_cast<notify_event *>(msg);

  assert(event != 0);

  switch(event->message_id)
  {
  case NOTIFY_DELETE_CLIENT:
    {
      push_session_ptr push_ss = 
        push_sessionmgr::instance()->find(event->session_id);
      if (push_ss)
      {
        push_ss->client()->close();
      }else
      {
        event_log->debug("reactor event handler not find session: %d",
                         event->session_id);
      }
    }
    break;
  case NOTIFY_RESUME_HANDLE:
    {
      pull_session_ptr pull_ss = 
        pull_sessionmgr::instance()->find(event->session_id);
      if (pull_ss)
      {
        pull_ss->resume_handler();
      }else
      {
        event_log->debug("reactor event handler not find pull session: %d",
                         event->session_id);
      }
    }
    break;
  default:
    assert(0);
  }
  delete event;

  return 0;
}
