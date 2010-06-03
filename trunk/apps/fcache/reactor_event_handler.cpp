#include "reactor_event_handler.h"

#include <ndk/logger.h>

static ndk::logger *event_log = ndk::log_manager::instance()->get_logger("root.event");

int reactor_event_handler::handle_msg(void *msg)
{
  notify_event *event = reinterpret_cast<notify_event *>(msg);
  http_session *hs = http_sessionmgr::instance()->find(event->session_id);
  if (hs)
  {
    event_log->debug("reactor event handler find session: %d",
                     event->session_id);
    http_client *hc = hs->client();
    assert(hc != 0);
    if (hc) delete hc;
  }else
  {
    event_log->debug("reactor event handler not find session: %d",
                     event->session_id);
  }
  delete event;
  return 0;
}
