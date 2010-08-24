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

#include "predefined.h"

class pipe_buffer;

/**
 * @class file_info
 * 
 * @brief
 */
class file_info 
{
public:
  enum
  {
    ST_UNCOMPLETED  = 1L    << 0,
    ST_COMPLETED    = 1L    << 1
  };
public:
  file_info()
    : status_(ST_UNCOMPLETED),
    fd_ref_count_(0),
    file_handle_(NDK_INVALID_HANDLE),
    mtime_(0),
    length_(0),
    pipe_buffers_(0),
    bitmap_(0)
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
  { 
    this->length_ = val; 
    int chunks = this->length_/PIPE_CHUNK_SIZE + 1;
    this->pipe_buffers_ = new pipe_buffer *[chunks];
    this->bitmap_ = new char[chunks];
    for (int i = 0; i < chunks; ++i)
    {
      this->pipe_buffers_[i] = 0;
      this->bitmap_[i] = '0';
    }
  }

  //
  inline int status()
  { return this->status_; }
  //
  inline void status(int val)
  { this->status_ = val; }

  //
  inline time_t mtime()
  { return this->mtime_; }
  //
  inline void mtime(const time_t val)
  { this->mtime_ = val; }

  //
  inline std::string &etag()
  { return this->etag_; }
  //
  inline void etag(const std::string &val)
  { this->etag_ = val; }

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
    if (this->fd_ref_count_ == 0 
        && this->file_handle_ != NDK_INVALID_HANDLE)
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
  // == Method
  pipe_buffer *find_pipe_buffer(int chunk_id);
  void insert_pipe_buffer(pipe_buffer *);
  pipe_buffer *release_pipe_buffer(int bit);

  void set_bitmap(int );

  //
  bool file_info::check_file_completed();
private:
  int status_;
  int fd_ref_count_;
  ndk::ndk_handle file_handle_;

  time_t mtime_;

  // file length
  int64_t length_;

  pipe_buffer **pipe_buffers_;

  char *bitmap_;

  std::string url_;

  std::string filename_;

  std::string etag_;

  ndk::thread_mutex fd_ref_count_mtx_;

  ndk::thread_mutex pipe_buffers_mtx_;

  ndk::thread_mutex bitmap_mtx_;
};
typedef boost::shared_ptr<file_info> file_info_ptr;

#endif // FILE_INFO_H_

