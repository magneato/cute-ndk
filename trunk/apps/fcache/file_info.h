// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-24 22:01
 */
//========================================================================

#ifndef FILE_INFO_H_
#define FILE_INFO_H_

#include <string>
#include <boost/shared_ptr.hpp>

#include <ndk/types.h>
#include <ndk/guard.h>
#include <ndk/thread_mutex.h>

/**
 * @class file_info
 * 
 * @brief
 */
class file_info 
{
public:
  file_info()
    : fd_ref_count_(0),
    file_handle_(NDK_INVALID_HANDLE),
    mtime_(0),
    length_(0)
  { }

  //
  inline std::string &url()
  { return this->url_; }
  //
  inline void url(const std::string &val)
  { this->url_ = val; }

  //
  inline std::string &filename()
  { return this->filename_; }
  //
  inline void filename(const std::string &val)
  { this->filename_ = val; }

  //
  inline int64_t length()
  { return this->length_; }
  //
  inline void length(const int64_t val)
  { this->length_ = val; }

  //
  inline time_t mtime()
  { return this->mtime_; }
  //
  inline void mtime(const time_t val)
  { this->mtime_ = val; }

  //
  inline void incr_fd_ref()
  { 
    ndk::guard<ndk::thread_mutex> g(this->fd_ref_count_mtx_);
    ++this->fd_ref_count_; 
  }
  //
  inline void decr_fd_ref()
  { 
    ndk::guard<ndk::thread_mutex> g(this->fd_ref_count_mtx_);
    --this->fd_ref_count_; 
    if (this->fd_ref_count_ == 0 && this->file_handle_ != NDK_INVALID_HANDLE)
    {
      ::close(this->file_handle_);
      this->file_handle_ = NDK_INVALID_HANDLE;
    }
  }

  //
  inline ndk::ndk_handle file_handle()
  { return this->file_handle_; }
  //
  inline void file_handle(const ndk::ndk_handle val)
  { this->file_handle_ = val; }

  //
  inline int64_t e_pos()
  { return this->e_pos_; }

  inline void e_pos(const int64_t val)
  { this->e_pos_ = val; }
private:
  int fd_ref_count_;
  ndk::ndk_handle file_handle_;

  time_t mtime_;

  // file length
  int64_t length_;
  int64_t e_pos_;

  std::string url_;

  std::string filename_;

  ndk::thread_mutex fd_ref_count_mtx_;
};
typedef boost::shared_ptr<file_info> fileinfo_ptr;

#endif // FILE_INFO_H_

