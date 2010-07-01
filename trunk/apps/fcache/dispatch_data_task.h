// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-24 22:01
 */
//========================================================================

#ifndef DISPATCH_DATA_TASK_H_
#define DISPATCH_DATA_TASK_H_

#include <ndk/task.h>

#include "http_client.h"
#include "transfer_agent.h"

/**
 * @class dispatch_data_task
 * 
 * @brief
 */
class dispatch_data_task : public ndk::task, public ndk::singleton<dispatch_data_task>
{
public:
  class dispatch_job
  {
  public:
    dispatch_job()
      : stopped(0),
      session_id(-1),
      bytes_to_send_per_timep(0),
      content_length_(0),
      transfer_bytes_(0),
      client(0),
      transfer_agent_(0),
      last_check_bandwidth_time(0)
    { }

    ~dispatch_job()
    { }
    
  public:
    int stopped;
    int session_id;
    int bytes_to_send_per_timep;
    uint64_t content_length_;
    uint64_t transfer_bytes_;
    http_client *client;
    transfer_agent *transfer_agent_;
    ndk::time_value last_check_bandwidth_time;
  };
public:
  dispatch_data_task()
  : task_idle_(1),
  dispatch_queue_not_empty_cond_(dispatch_queue_mtx_)
  { }

  int open();

  virtual int svc();

  int out_of_bandwidth(dispatch_job *job, ndk::time_value &now);

  void push_job(dispatch_job *job);

  void delete_client(int sid);
protected:

  void dispatch_data(void);

  void delete_job_i(dispatch_job *job);
private:
  volatile int task_idle_;

  ndk::time_value poll_before_;
  ndk::time_value poll_after_;
  ndk::time_value diff_time_;

  typedef std::deque<dispatch_job *> dispatch_queue_t;
  typedef std::deque<dispatch_job *>::iterator dispatch_queue_itor;

  dispatch_queue_t dispatch_queue_;
  ndk::thread_mutex dispatch_queue_mtx_;
  ndk::condition<ndk::thread_mutex> dispatch_queue_not_empty_cond_;
};

#endif // DISPATCH_DATA_TASK_H_

