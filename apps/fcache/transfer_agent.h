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

/**
 * @class transfer_agent
 * 
 * @brief
 */
class transfer_agent
{
public:
  transfer_agent() { }

  virtual int open(const fileinfo_ptr &finfo) = 0;

  virtual ~transfer_agent() {}

  virtual int transfer_data(int max_size, int &transfer_bytes) = 0;

  virtual int close() = 0;
};

#endif // TRANSFER_AGENT_H_

