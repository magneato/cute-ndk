// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-06-01 22:01
 */
//========================================================================

#ifndef HTTP_SESSIONMGR_H_
#define HTTP_SESSIONMGR_H_

#include <ndk/event_handler.h>
#include <ndk/singleton.h>

#include <map>

#include "http_session.h"

/**
 * @class http_sessionmgr
 *
 * @brief http sessionmgr
 */
class http_sessionmgr : public ndk::singleton<http_sessionmgr>
{
public:
  http_sessionmgr()
  : session_ids_(0)
  { }

  // return session id.
  inline int insert(http_session *hs)
  {
    std::pair<session_list_itor, bool> ret = 
      this->session_list_.insert(std::make_pair(hs->session_id(),
                                                hs));
    assert(ret.second == true);
    return ret.second ? 0 : -1;
  }

  inline http_session *find(int sid)
  {
    if (sid < 0) return 0;
    session_list_itor itor = this->session_list_.find(sid);
    if (itor == this->session_list_.end())
      return 0;
    return itor->second;
  }

  inline http_session *remove(int sid)
  {
    session_list_itor itor = this->session_list_.find(sid);
    if (itor == this->session_list_.end())
      return 0;
    http_session *hs = itor->second;
    this->session_list_.erase(itor);
    return hs;
  }

  inline int alloc_sessionid()
  {
    return this->session_ids_++;
  }

  inline size_t size()
  {
    return this->session_list_.size();
  }
private:
  int session_ids_;

  typedef std::map<int, http_session *> session_list_t;
  typedef std::map<int, http_session *>::iterator session_list_itor;
  session_list_t session_list_;
};

#endif // HTTP_SESSIONMGR_H_ 

