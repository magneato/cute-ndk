#include "read_data_task.h"

int read_data_task::svc()
{
  ndk::message_block *mb = 0;
  ndk::time_value *wait_time = 0;
  int result = 0;
  while (1)
  {
    if (this->read_task_is_idle_)
      wait_time = 0;
    else
      wait_time = &ndk::time_value::zero;

    result = this->getq_n(mb, -1, wait_time);
    if (result <= 0 || mb == 0)
      continue;
    assert(mb != 0);

    // 1. 
    this->proc_msg(mb);

    // 2. 
    this->handle_read_jobs();
  }
  return 0;
}
void read_data_task::proc_msg(message_block *mb_list)
{
  message_block *mb = mb_list;
  for (; mb != 0; mb = mb_list->next())
  {
    switch(mb->data_type())
    {
      // add a new job.
    case read_job_info::add_read_job:
      {
        ndk::guard<ndk::thread_mutex> g(this->read_job_queue_mutex_);
        this->read_job_queue_.push_back(reinterpret_cast<read_job_info*>(mb->data()));
        continue;
      }
      break;
      // process exception.
    default:
      log->error("get an unknow job type [%d]!", mb->data_type());
      break;
    }
    mb->clean(); delete mb;
  }
}
void read_data_task::delete_read_job(read_job_info *info)
{
  ndk::guard<ndk::thread_mutex> g(this->read_job_queue_mutex_);
  this->delete_read_job_i(info);
}
void read_data_task::delete_read_job_i(read_job_info *info)
{
  read_job_queue_itor itor = this->read_job_queue_.begin();
  for (; itor != this->read_job_queue_.end(); )
  {
    if (itor->session_id == info->session_id)
    {
      delete *itor;
      this->read_job_queue_.erase(itor);
      break;
    }
  }
}
void read_data_task::handle_read_jobs()
{
  ndk::guard<ndk::thread_mutex> g(this->read_job_queue_mutex_);
  read_job_queue_itor itor = this->read_job_queue_.begin();
  for (; itor != this->read_job_queue_.end(); )
  {
    read_job_info *info = *itor;
    if (info->has_job)
    {
      assert(info->data_handler 
             && info->data_block 
             && info->data_block->space() > 0);
      int ret = info->file->read(info->data_block->wr_ptr(), 
                                    info->bytes_to_read);
      prod_result result;
      info->data_handler->read_complete(result);
      info->has_job = 0;
    }
  }
  //
  this->read_task_is_idle_ = 1;
  return ;
}

