#include "dispatch_data_task.h"
#include "reactor_event_handler.h"
#include "errno.h"
#include <ndk/logger.h>

static ndk::logger *dispatch_log = ndk::log_manager::instance()->get_logger("root.dispatch");

#define TIME_PIECE     25

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
      ndk::guard<ndk::thread_mutex> g(this->change_list_mtx_);
      while (this->change_list_.empty())
      {
        // must be a loop
        this->change_list_not_empty_cond_.wait();
      }
    }

    this->check_change_list();

    this->dispatch_data();
  }
  return 0;
}
void dispatch_data_task::check_change_list()
{
  ndk::guard<ndk::thread_mutex> g(this->change_list_mtx_);
  if (!this->change_list_.empty())
  {
    change_list_itor itor = this->change_list_.begin();
    for (; itor != this->change_list_.end(); ++itor)
    {
      switch((*itor)->type_)
      {
      case dispatch_job::add_job:
        {
          this->dispatch_queue_.push_back((*itor));
          (*itor)->last_check_bandwidth_time_.update();
          break;
        }
      case dispatch_job::del_job:
        {
          this->delete_client_i((*itor)->session_id_);
          delete *itor;
          break;
        }
      default:
        assert(0);
      }
    }
    this->change_list_.clear();
  }
}
int dispatch_data_task::out_of_bandwidth(dispatch_job *job, 
                                         ndk::time_value &now)
{
  ndk::time_value diff_time = now - job->last_check_bandwidth_time_;
  if ((diff_time.msec() > 1000))
  {
    job->last_check_bandwidth_time_ = now;
    job->bytes_to_send_per_timep_ = 0;
    return 1;
  }
  return 0;
}
void dispatch_data_task::push_job(dispatch_job *job)
{
  ndk::guard<ndk::thread_mutex> g(this->change_list_mtx_);
  this->change_list_.push_back(job);
  this->change_list_not_empty_cond_.signal();
}
void dispatch_data_task::delete_client(int sid)
{
  dispatch_data_task::dispatch_job *job =
    new dispatch_data_task::dispatch_job(dispatch_data_task::dispatch_job::del_job);
  job->session_id_ = sid;
  ndk::guard<ndk::thread_mutex> g(this->change_list_mtx_);
  this->change_list_.push_back(job);
  this->change_list_not_empty_cond_.signal();
}
void dispatch_data_task::delete_client_i(int sid)
{
  dispatch_queue_itor itor = this->dispatch_queue_.begin();
  for (; itor != this->dispatch_queue_.end(); ++itor)
  {
    if ((*itor)->session_id_ == sid)
    {
      if ((*itor)->transfer_agent_)
      {
        if ((*itor)->transfer_agent_->close() != 0)
          // add by cuisw 2010-06-24
        {
          /**
           * aio task may be not processed. then we should keep checking 
           * in <svc> loop.
           */
          (*itor)->close_pending_ = 1;
          return ;
        }
        delete (*itor)->transfer_agent_;
        // add end
      }
      //(*itor)->client_->transfer_bytes((*itor)->transfer_bytes_);
      dispatch_log->trace("delete client [%d]", (*itor)->session_id_);
      delete *itor;
      this->dispatch_queue_.erase(itor);
      return ;
    }
  }
}
void dispatch_data_task::dispatch_data(void)
{
  this->poll_before_.update();

  //this->dispatch_queue_mtx_.acquire();

  int result = 0;
  size_t stopped_count = 0;
  for (dispatch_queue_itor itor = this->dispatch_queue_.begin(); 
       itor != this->dispatch_queue_.end(); )
  {
    dispatch_job *job = *itor;
    if (job->close_pending_)
      // keep checking closed or not.
    {
      if ((*itor)->transfer_agent_->close() == 0)
      {
        delete (*itor)->transfer_agent_;
        delete job;
        itor = this->dispatch_queue_.erase(itor);
        continue;
      }
      dispatch_log->trace("release an unpointed job %p", job); 
      goto LOOP_TAIL;
    }
    if (job->stopped_)
    {
      ++stopped_count;
      dispatch_log->trace("session %d stopped!", job->session_id_);
      goto LOOP_TAIL;
    }else if (this->out_of_bandwidth(job, this->poll_before_))
    {
      //dispatch_log->trace("[%d] out of bandwidth!", job->session_id_);
      goto LOOP_TAIL;
    }

    job->bytes_to_send_per_timep_ += (job->bandwidth_*1024/(1000/TIME_PIECE))/8;

    for (; job->bytes_to_send_per_timep_ > 0; )
    {
      int transfer_bytes = 0;
      result = job->transfer_agent_->transfer_data(job->client_->get_handle(),
                                                   job->bytes_to_send_per_timep_,
                                                   transfer_bytes);
      // 
      job->bytes_to_send_per_timep_ -= transfer_bytes;
      job->transfer_bytes_ += transfer_bytes;
      job->client_->transfer_bytes(job->transfer_bytes_);

      if (result < 0 && result != -1)
        job->client_->session_desc(my_strerr(-result));

      if (job->transfer_bytes_ == job->content_length_)
      {
        result = -1;
        dispatch_log->rinfo("[%d] transfer data end! total %lld bytes",
                            job->session_id_,
                            job->transfer_bytes_);
      }
      if (result < 0)
      {
        job->stopped_ = 1;
        this->delete_job_i(job);
        goto LOOP_TAIL;
      }else if (result == 0)
      {
        dispatch_log->trace("%d blocked ....", job->session_id_);
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

  //this->dispatch_queue_mtx_.release();

  this->poll_after_.update();
  this->diff_time_ = this->poll_after_ - poll_before_;
  int msec = TIME_PIECE - this->diff_time_.msec();
  if (msec > 5)
    ndk::sleep(ndk::time_value(0, (msec-3)*1000));
  return ;
}
void dispatch_data_task::delete_job_i(dispatch_job *job)
{
  if (ndk::reactor::instance()->notify(reactor_event_handler::instance(),
                                       new notify_event(0, job->session_id_)) != 0)
  {
    dispatch_log->error("notify session %d failed [%s]",
                        job->session_id_,
                        strerror(errno));
  }
}
