// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-08-20 18:06
 */
//========================================================================

#ifndef QUEUE_PULL_SESSIONMGR_H_
#define QUEUE_PULL_SESSIONMGR_H_

#include <ndk/singleton.h>

#include <set>
#include <string>
#include <utility>

/**
 * @class queue_pull_sessionmgr
 *
 * @brief queue pulling sessionmgr
 */
class queue_pull_sessionmgr : public ndk::singleton<queue_pull_sessionmgr>
{
public:
  queue_pull_sessionmgr()
  { }

  // return session id.
  inline int insert(const std::string &url)
  {
    std::pair<session_list_itor, bool> result 
      = this->session_list_.insert(url);
    assert(result.second == true);
    return 0;
  }

  inline int find(const std::string &url)
  {
    session_list_itor itor = this->session_list_.find(url);
    if (itor == this->session_list_.end())
      return -1;
    return 0;
  }

  inline void remove(const std::string &url)
  {
    session_list_itor itor = this->session_list_.find(url);
    if (itor == this->session_list_.end())
      assert(0);
    this->session_list_.erase(url);
  }
private:
  typedef std::set<std::string> session_list_t;
  typedef std::set<std::string>::iterator session_list_itor;
  session_list_t session_list_;
};

#endif // QUEUE_PULL_SESSIONMGR_H_ 

