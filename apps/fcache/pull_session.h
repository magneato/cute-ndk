// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-24 22:01
 */
//========================================================================

#ifndef PULL_SESSION_H_
#define PULL_SESSION_H_

#include <boost/shared_ptr.hpp>

#include "session.h"
#include "http_client.h"

class pull_agent;

/**
 * @class pull_session
 * 
 * @brief
 */
class pull_session : public session
{
public:
  pull_session(int sid)
    : session(sid),
    push_session_id_(-1)
  { }

  ~pull_session()
  { }

  inline int push_session_id(void)
  { return this->push_session_id_; }

  inline void push_session_id(int val)
  { this->push_session_id_ = val; }

  inline std::string &url_for_pulling(void)
  { return this->url_for_pulling_; }

  inline void url_for_pulling(const std::string &val)
  { this->url_for_pulling_ = val; }

  inline void pulling_agent(pull_agent *val)
  { this->pull_agent_ = val; }

  inline pull_agent *pulling_agent()
  { return this->pull_agent_; }

  // == Method
  virtual void release();

  int resume_handler();
private:
  int push_session_id_;

  pull_agent *pull_agent_;

  std::string url_for_pulling_;
};
typedef boost::shared_ptr<pull_session> pull_session_ptr;

#endif // PULL_SESSION_H_

