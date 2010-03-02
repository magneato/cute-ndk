#include "ndk/thread_manager.h"
#include "ndk/strace.h"
#include "ndk/task_base.h"
#include "ndk/global_macros.h"

namespace ndk
{
int thread_manager::append_thr(thread_t thr_id,
                               hthread_t thr_handle,
                               int thr_state,
                               int grp_id,
                               int flags,
                               task_base *task)
{
  STRACE("thread_manager");
  if (thr_id ==  0) return -1;
  if (grp_id == -1) return -1;
  thread_descriptor *new_thr_desc = new thread_descriptor;
  if (new_thr_desc == 0) return -1;
  new_thr_desc->grp_id_    = grp_id;
  new_thr_desc->thr_state_ = thr_state;
  new_thr_desc->thr_flags_ = flags;
  new_thr_desc->thr_id_    = thr_id;
  new_thr_desc->task_      = task;
  new_thr_desc->thr_mgr_   = this;
  new_thr_desc->thr_handle_ = thr_handle;

  guard<thread_mutex> g(this->thr_table_mutex_);
  thr_table_itor t_itor = this->thr_table_.find(grp_id);
  if (t_itor == this->thr_table_.end())
  {
    thr_list_t thr_list;
    thr_list.push_back(new_thr_desc);	
    this->thr_table_.insert(std::make_pair(grp_id, thr_list));
  }else
  {
    t_itor->second.push_back(new_thr_desc);
  }
  return 0;
}
void thread_manager::remove_thr_i(thread_t thr_id, int grp_id)
{
  STRACE("thread_manager");
  thr_table_itor t_itor = this->thr_table_.find(grp_id);
  if (t_itor == this->thr_table_.end())
    return ;
  thr_list_itor  l_itor = t_itor->second.begin();
  for (; l_itor != t_itor->second.end(); ++l_itor)
  {
    if (thread::thr_equal((*l_itor)->thr_id_, thr_id))
    {
      delete(*l_itor); // first
      t_itor->second.erase(l_itor);
      if (t_itor->second.empty())
        this->thr_table_.erase(t_itor);
      return;
    }
  }
}
void thread_manager::remove_thr_i(thread_t thr_id)
{
  STRACE("thread_manager");
  thr_table_itor t_itor = this->thr_table_.begin();
  thr_list_itor  l_itor;
  for (; t_itor != this->thr_table_.end(); ++t_itor)
  {
    l_itor = t_itor->second.begin();
    for (; l_itor != t_itor->second.end(); ++l_itor)
    {
      if (thread::thr_equal((*l_itor)->thr_id_, thr_id))
      {
        delete(*l_itor); // first
        t_itor->second.erase(l_itor);
        if (t_itor->second.empty())
          this->thr_table_.erase(t_itor);
        return;
      }
    }
  }
}
int thread_manager::get_grp_id(thread_t thr_id)
{
  STRACE("thread_manager");
  guard<thread_mutex> g(this->thr_table_mutex_);
  thread_descriptor *td = this->find_thread_i(thr_id);
  if (td)
  {
    return td->grp_id_;
  }
  return -1;
}
size_t thread_manager::thr_count()
{
  STRACE("thread_manager");
  guard<thread_mutex> g(this->thr_table_mutex_);
  int thr_count = 0;
  thr_table_itor t_itor = this->thr_table_.begin();
  for (; t_itor != this->thr_table_.end(); ++t_itor)
  {
    thr_count += t_itor->second.size();
  }
  return thr_count;
}
int thread_manager::wait()
{
  STRACE("thread_manager");
  thr_list_t term_thr_list_copy;
  int thr_count = 0;
  int term_thr_list_copy_size = 0;
  int diff_thr_term_and_thr_table = 0;
  this->update_thr_term_list_copy(term_thr_list_copy, thr_count);
  //printf("+++++++++++++++ thr_coutn = %d size = %d\n", thr_count, term_thr_list_copy.size());
  while (!term_thr_list_copy.empty())
  {
    term_thr_list_copy_size = term_thr_list_copy.size();
    diff_thr_term_and_thr_table = thr_count - term_thr_list_copy_size;
    //start to join threads
    thr_list_itor  l_itor = term_thr_list_copy.begin();
    for (; l_itor != term_thr_list_copy.end();)
    {
      if (NDK_BIT_DISABLED((*l_itor)->thr_flags_, thread::thr_detach)
          && NDK_BIT_ENABLED((*l_itor)->thr_flags_, thread::thr_join))
      {
        thread::join((*l_itor)->thr_handle_);
        int new_diff = this->thr_count() - term_thr_list_copy_size ;
        //printf("---------------- join %lu diff = %d\n",(*l_itor)->thr_id_, new_diff);
        this->remove_thr_i((*l_itor)->thr_id_,(*l_itor)->grp_id_);
        if (new_diff > diff_thr_term_and_thr_table)
        {
          //thread_t thr_id =(*l_itor)->thr_id_;
          //int grp_id      =(*l_itor)->grp_id_;
          term_thr_list_copy.clear();
          this->update_thr_term_list_copy(term_thr_list_copy, thr_count);
          break; // go to update term_thr_list_copy_size
        }
      }
      l_itor = term_thr_list_copy.erase(l_itor);
      --term_thr_list_copy_size;
    }
  }
  return 0; 
}
void thread_manager::update_thr_term_list_copy(thr_list_t &term_thr_list_copy, 
                                               int &thr_count)
{
  STRACE("thread_manager");
  guard<thread_mutex> g(this->thr_table_mutex_);
  thr_table_itor t_itor = this->thr_table_.begin();
  thr_list_itor  l_itor;
  thr_count = 0;
  for (; t_itor != this->thr_table_.end();)
  {
    l_itor = t_itor->second.begin();
    for (; l_itor != t_itor->second.end(); ++l_itor)
    {
      thr_count++;
      if (NDK_BIT_ENABLED((*l_itor)->thr_flags_, thread::thr_detach)
          && NDK_BIT_DISABLED((*l_itor)->thr_flags_, thread::thr_join))
      {
        // remove it
        delete(*l_itor);
        l_itor = t_itor->second.erase(l_itor);
      }else
        term_thr_list_copy.push_back(*l_itor);
    }
    // check it empty or not
    if (t_itor->second.empty())
      this->thr_table_.erase(t_itor++);
    else
      ++t_itor;
  }
} // release lock
int thread_manager::wait_task(task_base *task)
{
  STRACE("thread_manager");
  if (task == 0) return -1;
  int grp_id = task->get_grp_id();
  if (grp_id == -1) return -1;

  // lock scope
  this->thr_table_mutex_.acquire();
  thr_table_itor t_itor = this->thr_table_.find(grp_id);
  if (t_itor == this->thr_table_.end())
  {
    this->thr_table_mutex_.release();
    return -1;
  }
  thr_list_t thr_list(t_itor->second);
  this->thr_table_mutex_.release();
  // unlock

  thr_list_itor l_itor = thr_list.begin();
  for (; l_itor != thr_list.end(); ++l_itor)
  {
    NDK_SET_BITS((*l_itor)->thr_state_, thread_manager::thr_joining);
    if (NDK_BIT_ENABLED((*l_itor)->thr_flags_, thread::thr_join))
      thread::join((*l_itor)->thr_id_);
    NDK_SET_BITS((*l_itor)->thr_state_, thread_manager::thr_idle);
    // to ensure realtime of thr_count
    this->remove_thr_i((*l_itor)->thr_id_, grp_id);  
  }
  return 0; 
}
thread_descriptor* thread_manager::find_thread_i(thread_t thr_id)
{
  STRACE("thread_manager");
  thr_table_itor t_itor = this->thr_table_.begin();
  thr_list_itor l_itor;
  for (; t_itor != this->thr_table_.end(); ++t_itor)
  {
    l_itor = t_itor->second.begin();
    for (; l_itor != t_itor->second.end(); ++l_itor)
    {
      if (thread::thr_equal((*l_itor)->thr_id_, thr_id))
      {
        return(*l_itor);
      }
    }
  }
  return 0;
}
thread_descriptor* thread_manager::find_thread_by_handle(hthread_t thr_handle)
{
  STRACE("thread_manager");
  thr_table_itor t_itor = this->thr_table_.begin();
  thr_list_itor l_itor;
  for (; t_itor != this->thr_table_.end(); ++t_itor)
  {
    l_itor = t_itor->second.begin();
    for (; l_itor != t_itor->second.end(); ++l_itor)
    {
      if ((*l_itor)->thr_handle_, thr_handle)
      {
        return(*l_itor);
      }
    }
  }
  return 0;
}
thread_descriptor* thread_manager::find_thread_i(thread_t thr_id, 
                                                 int grp_id)
{
  STRACE("thread_manager");
  thr_table_itor t_itor = this->thr_table_.find(grp_id);
  if (t_itor != this->thr_table_.end())
  {
    thr_list_itor l_itor;
    l_itor = t_itor->second.begin();
    for (; l_itor != t_itor->second.end(); ++l_itor)
    {
      if (thread::thr_equal((*l_itor)->thr_id_, thr_id))
      {
        return(*l_itor);
      }
    }
  }
  return 0;
}
int thread_manager::set_thr_state(thread_t thr_id, size_t thr_state)
{
  STRACE("thread_manager");
  guard<thread_mutex> g(this->thr_table_mutex_);
  thread_descriptor *td = this->find_thread_i(thr_id);
  if (td)
  {
    NDK_SET_BITS(td->thr_state_, thr_state);
    return 0;
  }
  return -1;
}
int thread_manager::get_thr_state(thread_t thr_id, size_t &thr_state)
{
  STRACE("thread_manager");
  guard<thread_mutex> g(this->thr_table_mutex_);
  thread_descriptor *td = this->find_thread_i(thr_id);
  if (td)
  {
    thr_state = td->thr_state_;
    return 0;
  }
  return -1;
}
int thread_manager::cancel(hthread_t thr_handle, int async_cancel/* = 0*/)
{
  guard<thread_mutex> g(this->thr_table_mutex_);
  thread_descriptor *td = this->find_thread_by_handle(thr_handle);
  if (td == 0) return -1; // not be managed by thread_manager
  NDK_SET_BITS(td->thr_state_, thread_manager::thr_cancelled);
  if (async_cancel)
    return thread::cancel(thr_handle);
  return 0;
}
int thread_manager::cancel_grp(int grp_id, int async_cancel/* = 0*/)
{
  guard<thread_mutex> g(this->thr_table_mutex_);
  thr_table_itor t_itor = this->thr_table_.find(grp_id);
  if (t_itor != this->thr_table_.end())
  {
    thr_list_itor l_itor;
    l_itor = t_itor->second.begin();
    for (; l_itor != t_itor->second.end(); ++l_itor)
    {
      NDK_BIT_ENABLED((*l_itor)->thr_state_, 
                      thread_manager::thr_cancelled);
      if (async_cancel)
        thread::cancel((*l_itor)->thr_handle_);
    }
    return 0;
  }
  return -1;
}
int thread_manager::testcancel(thread_t thr_id, int async_cancel/* = 0*/)
{
  guard<thread_mutex> g(this->thr_table_mutex_);
  thread_descriptor *td = this->find_thread_i(thr_id);
  if (td == 0) return -1;
  if (async_cancel)
    thread::testcancel();
  return NDK_BIT_ENABLED(td->thr_state_, 
                         thread_manager::thr_cancelled) ? 0 : -1;
}
} // namespace ndk

