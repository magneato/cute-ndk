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
  mem_cache_transfer(uint64_t start_pos,
                     uint64_t content_len);

  virtual int open(const fileinfo_ptr &finfo);

  virtual ~mem_cache_transfer();

  virtual int transfer_data(ndk::ndk_handle h,
                            int max_size, 
                            int &transfer_bytes);

  virtual int close();
protected:
  int start_read();

  int close_i();
protected:
  size_t send_bytes_;

  uint64_t offset_;
  uint64_t content_length_;
  uint64_t start_pos_;
  uint64_t transfer_bytes_;

  ndk::cache_object *cache_obj_;

  std::string url_;
  std::string filename_;

  ndk::thread_mutex buffer_mtx_;
};

#endif // MEM_CACHE_TRANSFER_H_

