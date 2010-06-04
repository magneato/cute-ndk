#include "dispatch_data_task.h"
#include "reactor_event_handler.h"
#include <ndk/logger.h>

static ndk::logger *dispatch_log = ndk::log_manager::instance()->get_logger("root.dispatch");

#define TIME_PIECE    50

extern int g_bandwidth;

int dispatch_data_task::open()
{
  return this->activate(ndk::thread::thr_join, 1);
}
int dispatch_data_task::svc()
{
  while (1)
  {
    if (this->task_idle_)
    {
      ndk::guard<ndk::thread_mutex> g(this->dispatch_queue_mtx_);
      this->dispatch_queue_not_empty_cond_.wait();
    }

    this->dispatch_data();
  }
  return 0;
}
int dispatch_data_task::out_of_bandwidth(dispatch_job *job, 
                                         ndk::time_value &now)
{
  ndk::time_value diff_time = now - job->last_check_bandwidth_time;
  if ((diff_time.msec() > 1000))
  {
    job->last_check_bandwidth_time = now;
    job->bytes_to_send_per_timep = 0;
    return 1;
  }
  return 0;
}
void dispatch_data_task::push_job(dispatch_job *job)
{
  ndk::guard<ndk::thread_mutex> g(this->dispatch_queue_mtx_);
  this->dispatch_queue_.push_back(job);
  job->last_check_bandwidth_time.update();
  dispatch_queue_not_empty_cond_.signal();
}
void dispatch_data_task::delete_client(int sid)
{
  ndk::guard<ndk::thread_mutex> g(this->dispatch_queue_mtx_);
  dispatch_queue_itor itor = this->dispatch_queue_.begin();
  for (; itor != this->dispatch_queue_.end(); ++itor)
  {
    if ((*itor)->session_id == sid)
    {
      (*itor)->transfer_agent_->close();
      delete *itor;
      this->dispatch_queue_.erase(itor);
      return ;
    }
  }
}
void dispatch_data_task::dispatch_data(void)
{
  this->poll_before_.update();
  {
    //ndk::guard<ndk::thread_mutex> g(this->dispatch_queue_mtx_);
    this->dispatch_queue_mtx_.acquire();

    dispatch_queue_itor itor = this->dispatch_queue_.begin();
    int result = 0;
    size_t stopped_count = 0;
    for (; itor != this->dispatch_queue_.end(); )
    {
      dispatch_job *job = *itor;
      if (job->stopped)
      {
        ++stopped_count;
        dispatch_log->debug("session %d stopped!", job->session_id);
        goto LOOP_TAIL;
      }else if (this->out_of_bandwidth(job, this->poll_before_))
        goto LOOP_TAIL;

      job->bytes_to_send_per_timep += (g_bandwidth*1024/(1000/TIME_PIECE))/8;

      for (; job->bytes_to_send_per_timep > 0; )
      {
        int transfer_bytes = 0;
        result = job->transfer_agent_->transfer_data(job->client->get_handle(),
                                                     job->bytes_to_send_per_timep,
                                                     transfer_bytes);
        // 
        job->bytes_to_send_per_timep -= transfer_bytes;
        job->transfer_bytes_ += transfer_bytes;
        if (job->transfer_bytes_ == job->content_length_)
        {
          result = -1;
          dispatch_log->rinfo("transfer data end! total %lld bytes",
                              job->transfer_bytes_);
        }
        if (result < 0)
        {
          job->stopped = 1;
          this->delete_job_i(job);
          goto LOOP_TAIL;
        }else if (result == 0)
        {
          // blocked.
          break;
        }
      }

LOOP_TAIL:
      ++itor;
    } // end `for (; itor != this->dispatch_queue_.end(); ...'
    if (this->dispatch_queue_.empty()
        || 
        stopped_count == this->dispatch_queue_.size())
      this->task_idle_ = 1;
    else
      this->task_idle_ = 0;

    this->dispatch_queue_mtx_.release();
  }
  this->poll_after_.update();
  this->diff_time_ = this->poll_after_ - poll_before_;
  int msec = TIME_PIECE - this->diff_time_.msec();
  if (msec > 5)
  {
    ndk::sleep(ndk::time_value(0, (msec-3)*1000));
  }
}
void dispatch_data_task::delete_job_i(dispatch_job *job)
{
  if (ndk::reactor::instance()->notify(reactor_event_handler::instance(),
                                       new notify_event(0, job->session_id)) != 0)
  {
    dispatch_log->error("notify session %d failed [%s]",
                     job->session_id,
                     strerror(errno));
  }
}
