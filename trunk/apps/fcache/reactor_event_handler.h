// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-06-01 22:01
 */
//========================================================================

#ifndef REACTOR_EVENT_HANDLER_H_
#define REACTOR_EVENT_HANDLER_H_

#include <ndk/event_handler.h>
#include <ndk/singleton.h>

#define  NOTIFY_DELETE_CLIENT           1001
#define  NOTIFY_RESUME_HANDLE           1002

/**
 * @brief notify event class
 */
class notify_event
{
public:
  notify_event(int msgid, int sid)
    : message_id(msgid),
    session_id(sid)
  {  }

  ~notify_event()
  {  }
public:
  int message_id;
  int session_id;
};
/**
 * @class reactor_event_handler
 *
 * @brief
 */
class reactor_event_handler : public ndk::event_handler, public ndk::singleton<reactor_event_handler>
{
public:
  reactor_event_handler()
  { }

  virtual int handle_msg(void *msg);
};

#endif // REACTOR_EVENT_HANDLER_H_ 

