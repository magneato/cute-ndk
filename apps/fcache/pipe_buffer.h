// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-07-22 16:01
 */
//========================================================================

#ifndef PIPE_BUFFER_H_
#define PIPE_BUFFER_H_

#include <stdint.h>

#include <ndk/thread_mutex.h>
#include <ndk/asynch_file_io.h>

#include "file_info.h"
#include "predefined.h"

#ifdef DUMP_INFO
# include <ndk/object_trace.h>
#endif

/**
 * @class pipe_buffer
 * 
 * @brief
 */
class pipe_buffer : public ndk::asynch_handler
#ifdef DUMP_INFO
                    , public ndk::object_trace_mt<pipe_buffer>
#endif
{
public:
  enum
  {
    ST_SUSPEND       = 1L  << 0,
    ST_WRITING       = 1L  << 1,
  };
public:
  pipe_buffer(int chunk_id,
              size_t content_length, 
              const file_info_ptr &fileinfo);

  ~pipe_buffer();

  int open(); 

  size_t write(const char *bf, size_t len);

  size_t read(char *bf, size_t r_offset, size_t bytes_to_read);

  virtual void handle_aio_write(const ndk::aio_opt_t *aio_result);

  size_t size()
  { return this->buffer_size_; }

  int eof()
  { 
    ndk::guard<ndk::thread_mutex> g(this->buffer_mtx_);
    return this->wr_ptr_ < this->buffer_size_ ? 0 : 1; 
  }

  void status(int val)
  { 
    ndk::guard<ndk::thread_mutex> g(this->buffer_mtx_);
    this->status_ = val;
  }
  int status(void)
  { 
    ndk::guard<ndk::thread_mutex> g(this->buffer_mtx_);
    return this->status_;
  }

  int close(int sid);

#ifdef DUMP_INFO
  static void dump()
  {
    fprintf(stderr, "\npipe buffer malloc count = %d\n", 
            ndk::object_trace_mt<pipe_buffer>::malloc_count());
  }
#endif
  int chunk_id()
  { return this->chunk_id_; }
protected:
  ndk::ndk_handle handle_;

  int chunk_id_;

  int status_;

  //
  size_t ref_count_;

  // write offset
  size_t wr_ptr_;

  //
  size_t buffer_size_;

  //
  size_t bytes_to_write_;

  // buffer chunk.
  char *buffer_;

  file_info_ptr file_info_;

  ndk::thread_mutex buffer_mtx_;
};

#endif // PIPE_BUFFER_H_
