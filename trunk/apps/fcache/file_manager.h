// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-24 22:01
 */
//========================================================================

#ifndef FILE_MANAGER_H_
#define FILE_MANAGER_H_

#include <map>
#include <deque>
#include <string>
#include <ndk/singleton.h>

#include "file_info.h"

/**
 * @class file_manager
 * 
 * @brief
 */
class file_manager : public ndk::singleton<file_manager>
{
public:
  file_manager()
  { }

  ~file_manager()
  { }

  //
  inline int insert(file_info_ptr &finfo)
  {
    std::pair<file_map_iter, bool> result = 
      this->file_map_.insert(std::make_pair(finfo->url(), finfo));
    return result.second ? 0 : -1;
  }

  //
  inline file_info_ptr find(const std::string &url)
  {
    file_map_iter pos = this->file_map_.find(url);
    if (pos == this->file_map_.end())
      return file_info_ptr();
    return pos->second;
  }

  //
  inline void remove(const std::string &url)
  {
    this->file_map_.erase(url);
  }

  //
  std::deque<std::string> get_all_urls()
  {
    std::deque<std::string> l;
    file_map_iter pos = this->file_map_.begin();
    for (; pos != this->file_map_.end(); ++pos)
      l.push_back(pos->first);
    return l;
  }
private:
  typedef std::map<std::string, file_info_ptr> file_map_t;
  typedef std::map<std::string, file_info_ptr>::iterator file_map_iter;

  file_map_t file_map_;
};

#endif // FILE_MANAGER_H_

