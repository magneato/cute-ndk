// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-05-28 16:51
 */
//========================================================================

#ifndef MEM_CACHE_TRANSFER_H_
#define MEM_CACHE_TRANSFER_H_

#include "transfer_agent.h"

#include <ndk/cache_object.h>
#include <ndk/asynch_file_io.h>


/**
 * @class serial_file_transfer
 * 
 * @brief
 */
class mem_cache_transfer : public transfer_agent
{
public:
  mem_cache_transfer(int64_t start_pos,
                     int64_t content_len);

  virtual int open(const file_info_ptr &finfo);

  virtual ~mem_cache_transfer();

  virtual int transfer_data(ndk::ndk_handle h,
                            int max_size, 
                            int &transfer_bytes);

  virtual void start_transfer() {} ;

  virtual void stop_transfer() {} ;

  virtual int close();
protected:
  int start_read();

  int close_i();
protected:
  size_t send_bytes_;

  int64_t offset_;
  int64_t content_length_;
  int64_t begin_pos_;
  int64_t transfer_bytes_;

  ndk::cache_object *cache_obj_;

  std::string url_;
  std::string filename_;

  ndk::thread_mutex buffer_mtx_;
};

#endif // MEM_CACHE_TRANSFER_H_

