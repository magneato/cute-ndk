// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-05-28 16:51
 */
//========================================================================

#ifndef SERIAL_FILE_TRANSFER_H_
#define SERIAL_FILE_TRANSFER_H_

#include "transfer_agent.h"

#include <ndk/asynch_file_io.h>

/**
 * @class serial_file_transfer
 * 
 * @brief
 */
class serial_file_transfer : public transfer_agent, ndk::asynch_handler
{
public:
  serial_file_transfer(int64_t start_pos,
                       int64_t bytes_to_transfer);

  virtual int open(const file_info_ptr &fileinfo);

  virtual ~serial_file_transfer();

  virtual int transfer_data(ndk::ndk_handle h,
                            int max_size, 
                            int &transfer_bytes);

  virtual void handle_aio_read(const ndk::aio_opt_t *aio_result);

  virtual void start_transfer() {} ;

  virtual void stop_transfer() {} ;

  virtual int close();
protected:
  int start_read(int fd_prio, int io_prio);

  int close_i();
protected:
  ndk::ndk_handle handle_;
  
  int aio_id_;

  // volatile int aio_inprogress_;
  volatile int eof_;
  volatile int file_io_error_;
  volatile int cancel_pendding_;

  size_t send_bytes_;
  size_t buffer_size_;
  size_t reserve_buffer_size_;

  int64_t offset_;
  int64_t content_length_;
  int64_t begin_pos_;
  int64_t transfer_bytes_;

  char *buffer_;
  // used for prefetch data.
  char *reserve_buffer_;

  //
  file_info_ptr fileinfo_;

  ndk::thread_mutex buffer_mtx_;
};

#endif // SERIAL_FILE_TRANSFER_H_

