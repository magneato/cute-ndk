// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-07-02 11:00
 */
//========================================================================

#ifndef DISTRIBUTE_TASK_H_
#define DISTRIBUTE_TASK_H_

#include <ndk/task.h>

#include "session.h"
#include "fcache_defines.h"

/**
 * @class distri_info
 * 
 * @brief
 */
class distri_info
{
public:
  distri_info()
    : session_id(INVALID_SESSION_ID),
    bandwidth(default_bandwidth) 
  { }

  bool is_out_of_bandwidth(ndk::time_value *now = 0);

  int bytes_to_send_per_timepiece();

  // The index of read data task array.
  int read_data_task_index_;

  // client session.
  session_id_t  session_id;

  // output bandwidth.
  int           bandwidth;    

  // 
  int           period_flux;
};

/**
 * @class distribute_task
 * 
 * @brief
 */
class distribute_task : public ndk::task
{
public:
  distribute_task()
  { }

  virtual int open(void *args/* = 0*/);

  virtual int svc();
protected:
};

#endif // DISTRIBUTE_TASK_H_

