#include "pull_agent.h"
#include "pull_session.h"
#include "pull_sessionmgr.h"

void pull_session::release()
{
  pull_sessionmgr::instance()->remove(this->session_id_);
}
int pull_session::resume_handler()
{
  if (this->pull_agent_)
  {
    return this->pull_agent_->resume();
  }
  return -1;
}
