#include "pull_session.h"
#include "pull_sessionmgr.h"

void pull_session::release()
{
  pull_sessionmgr::instance()->remove(this->session_id_);
}
