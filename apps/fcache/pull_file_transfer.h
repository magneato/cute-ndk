// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-05-28 16:51
 */
//========================================================================

#ifndef PULL_FILE_TRANSFER_H_
#define PULL_FILE_TRANSFER_H_

#include <string>
#include <stdint.h>

#include "file_info.h"
#include "transfer_agent.h"

class pipe_buffer;

/**
 * @class pull_file_transfer
 * 
 * @brief
 */
class pull_file_transfer : public transfer_agent
{
  enum
  {
    STOP_TRANSFER     = 1L  << 0,
    START_TRANSFER    = 1L  << 1
  };
public:
  pull_file_transfer(ndk::ndk_handle pull_file_handle,
                     int64_t begin_pos,
                     int64_t bytes_to_transfer,
                     int64_t remaind_length);

  virtual int open(const file_info_ptr &finfo);

  virtual ~pull_file_transfer();

  virtual int transfer_data(ndk::ndk_handle h,
                            int max_size, 
                            int &transfer_bytes);

  virtual void start_transfer()
  {
    ndk::guard<ndk::thread_mutex> g(this->state_mtx_);
    this->state_ = START_TRANSFER;
  }

  virtual void stop_transfer()
  {
    ndk::guard<ndk::thread_mutex> g(this->state_mtx_);
    this->state_ = STOP_TRANSFER;
  }

  virtual int close();

protected:
  int read_pipe_buffer();

  pipe_buffer *create_pipe_buffer();
private:
  int    chunk_id_;

  ndk::ndk_handle pull_file_handle_;

  int state_;

  int first_chunk_;

  size_t rd_offset_;

  size_t send_bytes_;

  size_t buffer_size_;

  char *buffer_;

  int64_t begin_pos_;
  int64_t bytes_to_transfer_;
  int64_t transfer_bytes_;
  int64_t remaind_length_;

  pipe_buffer *pipe_buffer_;

  file_info_ptr file_info_;

  ndk::thread_mutex state_mtx_;
};

#endif // PULL_FILE_TRANSFER_H_

