// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-07-11 19:34
 */
//========================================================================

#ifndef READ_DATA_TASK_H_
#define READ_DATA_TASK_H_

class read_job_info
{
public:
  enum
  {
    add_read_job  = 0x10,
  };
  ndk_handle  handle;

  data_productor *data_handler;
};
/**
 * @class read_data_task
 * 
 * @brief
 */
class read_data_task
{
public:
  read_data_task()
  {
  }

  virtual ~read_data_task()
  {
  }
};

#endif // READ_DATA_TASK_H_

