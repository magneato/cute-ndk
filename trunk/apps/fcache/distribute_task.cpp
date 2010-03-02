#include "distribute_task.h"

#define TIME_PIECE              (40)   // msec
#define TIME_PIECE_COUNT        (1000/TIME_PIECE)    

static ndk::ndk_log *log = ndk::log_manager::instance()->get_logger("distri_task");
bool distri_info::is_out_of_bandwidth(const ndk::time_value *now)
{
  ndk::time_value nt;
  if (now == 0)
  {
    nt = ndk::gettimeofday();
    now = &nt;
  }
  ndk::time_value diff_time = *now - this->last_check_bandwidth_time_;
  if (diff_time.msec() < 1000)
  {
    if (this->period_flux_*8 < this->bandwidth_*1024)
      return false;
  }
  this->current_output_bandwidth_ = (this->period_flux_*8*1000 / 
                                     diff_time.msec())/1024;
  this->period_flux_ = 0;
  this->last_check_bandwidth_time_ = *now;
  return true;
}
void distri_info::destroy(session *ss)
{
  if (ss)
  {
  }
  delete this;
}
distribute_task::distribute_task()
: distri_task_is_idle_(1) 
{
}
int distribute_task::open(void *)
{
  ;
}
int distribute_task::svc()
{
  ndk::message_block *mb = 0;
  ndk::time_value *wait_time = 0;
  int result = 0;
  while (1)
  {
    if (this->distri_task_is_idle_)
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
    this->do_distribute();
  }
  return 0;
}
void distribute_task::proc_msg(message_block *mb_list)
{
  message_block *mb = mb_list;
  for (; mb != 0; mb = mb_list->next())
  {
    switch(mb->data_type())
    {
      // add a new client.
    case distri_info::add_distr_job:
      {
        ndk::guard<ndk::thread_mutex> g(this->distri_queue_mutex_);
        this->distri_queue_.push_back(reinterpret_cast<distri_info *>(mb->data()));
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
int distribute_task::do_distribute()
{
  FD_ZERO(&this->write_fds_);
  this->write_fds_queue_.clear();
  this->max_write_fd_ = NDK_INVALID_HANDLE;
  return this->do_distribute_i();
}
int distribute_task::do_distribute_i()
{
  this->distri_queue_mutex_.acquire();
  ndk::time_value before_loop_time = ndk_os::gettimeofday();
  ndk::time_value &now = before_loop_time;

  distri_queue_itor itor = this->distri_queue_.begin();
  // loop for distributing every client
  for (; itor != this->distri_queue_.end(); )
  {
    distri_info *item = *itor;
    assert (item != 0);
    int result = 0;
    session *ss = session_mgr::instance()->get_session(item->session_id);
    if (ss == 0 || ss->status() != session::transporting)
    {
      item->destroy(ss);
      itor = this->distri_queue_.erase(itor);
      continue;
    }
    result = this->distri_one_job(item, ss);
    if (result == -1 || result == 2)
    {
      item->destroy(ss);
      itor = this->distri_queue_.erase(itor);
      continue;
    }
    ++itor; // switch to next client
  } // end of `for (; itor != this->distri_queue_.end(); )'
  ndk::time_value diff_time = ndk_os::gettimeofday() - before_loop_time;
  if (diff_time.msec() >= (TIME_PIECE - 3))
  {
    this->distri_queue_mutex_.release();
    return 0;
  }
  /**
   * We use select to escape remaind time.
   *
   */
  struct timeval tv;
  tv.tv_sec  = 0;
  tv.tv_usec = (TIME_PIECE - diff_time.msec() - 3) * 1000;
  int result = 0;
  if (this->max_write_fd_ == NDK_INVALID_HANDLE)
    this->distri_queue_mutex_.release();
  while (1)
  {
    fd_set wfds;
    FD_ZERO(wfds);
    std::memcpy(wfds, this->write_fds_, sizeof(this->write_fds_));
    result = ::select(this->max_write_fd_ + 1, 
                      0,
                      this->max_write_fd_ == NDK_INVALID_HANDLE ? 0 : &wfds,
                      0,
                      &tv);
    if (result < 0)
    { 
      if (ndk_os::last_error() == EINTR)
        continue;
      else
      {
        log->error("select error [%s]!", strerror(ndk_os::last_error()));
        break;
      }
    }else if (result > 0)
    {
      write_fds_queue_itor itor = this->write_fds_queue_.begin();
      for (; itor != this->write_fds_queue_.end(); )
      {
        distri_info *item = *itor;
        session *ss = session_mgr::instance()->get_session(item->session_id);
        if (ss == 0 || ss->status() != session::session_transporting)
        {
          if (ss) this->clear_write_fd(ss)
          this->delete_client_i(item);
          item->destroy(0);
          itor = this->write_fds_queue_.erase(itor); 
          continue;
        }
        result = this->handle_is_writeable(ss);
        if (result == -1)
        {
          this->clear_write_fd(ss);
          this->delete_client_i(item);
          item->destroy(ss);
          itor = this->write_fds_queue_.erase(itor); 
          continue;
        }else if (result == 0)
        {
          itor = this->write_fds_queue_.erase(itor); 
          continue;
        }
        result = this->distri_one_job(item, ss, 0);
        if (result == -1 || result == 2)
        {
          itor = this->write_fds_queue_.erase(itor); 
          this->delete_client_i(item);
          item->destroy(ss);
        }else if (result == 0)
        {
          this->clear_write_fd(ss);
          itor = this->write_fds_queue_.erase(itor); 
          continue;
        }
        ++itor;
      }
    }else  // timeout.
      break;  // escape over.
  } // while (1)
  //
  if (this->distri_queue_.empty())
    this->distri_task_is_idle_ = 1;
  else
    this->distri_task_is_idle_ = 0;
  //
  if (this->max_write_fd_ != NDK_INVALID_HANDLE)
    this->distri_queue_mutex_.release();
  return 0;
}
/** 
 * return 0: out of bandwidth or get data block failed
 *        1: distribute ok
 *        2: traffic eof.
 *       -1: error
 */
int distribute_task::distri_one_job(distri_info *item, 
                                    session *ss,
                                    int use_select)
{
  // @step 1. check bandwidth
  if (item->is_out_of_bandwidth(&now))
  {
    ss->current_output_bandwidth(item->current_output_bandwidth_);
    return 0;
  }

  int result = 0;

  // @step 2. get bandwidth per time-piece
  int bytes_to_send_per_timep = item->bytes_to_send_per_timepiece();
  for (int send_bytes_per_timep = 0; 
       send_bytes_per_timep < bytes_to_send_per_timep; )
  {
    // get data block
    if (item->data_block->length() == 0)
    {
      data_result = item->data_src->get_data_block();
      if (data_result.error != 0) // 
      {
        if (data_result.error != prod_result::opt_inprogress)
          return -1;
        return 0;
      }
      assert(data_result.data_block != 0);
    }
    result = ss->send_data(item->data_block->rd_ptr(), 
                           std::min(item->data_block->length(), send_bytes_one_time));
    assert(result != 0);  // only mb->length() == 0;
    if (result < 0)
    {
      if (ndk_os::last_error() == EWOULDBLOCK)
      {
        if (use_select)
          return this->append_select_write_queue(item);
        return 0;
      }
      return -1;
    }
    item->data_block->rd_ptr(result);
    item->period_flux += result;
    item->total_flux  += result;
    ss->statist_total_output_flux(result);
    if (item->send_eof())
      return 2;
    send_bytes_per_timep += result;
  } // end of `for (send_bytes_per_timep < bytes_to_send_per_timep)'
  return 1;
}
int distribute_task::append_select_write_queue(distri_info *item,
                                               session *ss)
{
  if (ss->sock_handler() == 0)
    return -1;
  if (this->write_fds_queue_.size() >= FD_SETSIZE)
    return 0;
  FD_SET(ss->sock_handler()->peer().get_handle(), &this->write_fds_);
  this->max_write_fd_ = std::max(this->max_write_fd_,
                                 ss->sock_handler()->peer().get_handle());
  this->write_fds_queue_.push_back(item);
  return 0;
}
int distribute_task::clear_write_fd(session *ss)
{
  if (ss->sock_handler() == 0)
    return -1;
  FD_CLR(ss->sock_handler()->peer().get_handle(), &this->write_fds_);
  return 0;
}
int distribute_task::handle_is_writeable(session *ss)
{
  if (ss->sock_handler() == 0)
    return -1;
  return FD_ISSET(ss->sock_handler()->peer().get_handle(), 
                  &this->write_fds_) ? 1 : 0;
}
void distribute_task::delete_client(distri_info *info)
{
  ndk::guard<thread_mutex> g(this->distri_queue_mutex_);
  this->delete_client_i(info);
}
void distribute_task::delete_client_i(distri_info *info)
{
  distri_queue_itor itor = this->distri_queue_.begin();
  for (; itor != this->distri_queue_.end(); )
  {
    if (itor->session_id == info->session_id)
    {
      itor->destroy(0);
      this->distri_queue_.erase(itor);
      break;
    }
  }
}
