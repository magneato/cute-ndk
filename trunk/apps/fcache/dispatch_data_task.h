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

#include <list>

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
    enum
    {
      add_job    = 1L << 0,
      del_job    = 1L << 1
    };
  public:
    dispatch_job(int type)
      : type_(type),
      stopped_(0),
      session_id_(-1),
      close_pending_(0),
      bandwidth_(0),
      bytes_to_send_per_timep_(0),
      content_length_(0),
      transfer_bytes_(0),
      client_(0),
      transfer_agent_(0),
      last_check_bandwidth_time_(0)
    { }

    ~dispatch_job()
    { }
    
  public:
    int type_;
    int stopped_;
    int session_id_;
    int close_pending_;
    int bandwidth_;
    int bytes_to_send_per_timep_;
    uint64_t content_length_;
    uint64_t transfer_bytes_;
    http_client *client_;
    transfer_agent *transfer_agent_;
    ndk::time_value last_check_bandwidth_time_;
  };
public:
  dispatch_data_task()
  : task_idle_(1),
  change_list_mtx_(),
  change_list_not_empty_cond_(change_list_mtx_)
  { }

  int open();

  virtual int svc();

  int out_of_bandwidth(dispatch_job *job, ndk::time_value &now);

  void push_job(dispatch_job *job);

  void delete_client(int sid);

protected:
  void check_change_list();

  void dispatch_data(void);

  void delete_client_i(int sid);

  void delete_job_i(dispatch_job *job);
private:
  volatile int task_idle_;

  ndk::time_value poll_before_;
  ndk::time_value poll_after_;
  ndk::time_value diff_time_;

  typedef std::list<dispatch_job *> dispatch_queue_t;
  typedef std::list<dispatch_job *>::iterator dispatch_queue_itor;

  typedef std::list<dispatch_job *> change_list_t;
  typedef std::list<dispatch_job *>::iterator change_list_itor;

  dispatch_queue_t dispatch_queue_;

  change_list_t change_list_;
  ndk::thread_mutex change_list_mtx_;
  ndk::condition<ndk::thread_mutex> change_list_not_empty_cond_;
};

#endif // DISPATCH_DATA_TASK_H_

