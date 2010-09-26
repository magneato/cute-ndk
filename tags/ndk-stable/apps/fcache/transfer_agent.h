// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-05-28 16:51
 */
//========================================================================

#ifndef TRANSFER_AGENT_H_
#define TRANSFER_AGENT_H_

#include <string>
#include <stdint.h>

#include "file_info.h"

#define ONCE_TRANSFER_PACKET_SIZE   4096

/**
 * @class transfer_agent
 * 
 * @brief
 */
class transfer_agent
{
public:
  transfer_agent() { }

  virtual int open(const file_info_ptr &finfo) = 0;

  virtual ~transfer_agent() {}

  virtual int transfer_data(ndk::ndk_handle h,
                            int max_size, 
                            int &transfer_bytes) = 0;

  virtual void start_transfer() = 0;

  virtual void stop_transfer() = 0;

  virtual int close() = 0;
};

#endif // TRANSFER_AGENT_H_

