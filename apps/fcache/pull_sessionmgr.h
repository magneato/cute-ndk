// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-06-01 22:01
 */
//========================================================================

#ifndef PULL_SESSIONMGR_H_
#define PULL_SESSIONMGR_H_

#include <ndk/singleton.h>

#include <map>

#include "pull_session.h"

/**
 * @class pull_sessionmgr
 *
 * @brief 
 */
class pull_sessionmgr : public ndk::singleton<pull_sessionmgr>
{
public:
  pull_sessionmgr()
  : session_ids_(1)
  { }

  // return session id.
  inline int insert(pull_session_ptr &hs)
  {
    std::pair<session_list_itor, bool> ret = 
      this->session_list_.insert(std::make_pair(hs->session_id(),
                                                hs));
    assert(ret.second == true);
    return ret.second ? 0 : -1;
  }

  inline pull_session_ptr find(int sid)
  {
    if (sid < 0) return pull_session_ptr();
    session_list_itor itor = this->session_list_.find(sid);
    if (itor == this->session_list_.end())
      return pull_session_ptr();
    return itor->second;
  }

  inline int remove(int sid)
  {
    session_list_itor itor = this->session_list_.find(sid);
    if (itor == this->session_list_.end())
      return -1;
    this->session_list_.erase(itor);
    return 0;
  }

  inline int release(int sid)
  {
    session_list_itor itor = this->session_list_.find(sid);
    if (itor == this->session_list_.end())
      return -1;
    //
    itor->second->release();
    this->session_list_.erase(itor);
    return 0;
  }

  inline int alloc_sessionid()
  {
    return this->session_ids_ += 2;
  }

  inline size_t size()
  {
    return this->session_list_.size();
  }
private:
  int session_ids_;

  typedef std::map<int, pull_session_ptr> session_list_t;
  typedef std::map<int, pull_session_ptr>::iterator session_list_itor;
  session_list_t session_list_;
};

#endif // PULL_SESSIONMGR_H_ 

