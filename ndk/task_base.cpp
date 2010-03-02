#include "ndk/task_base.h"

namespace ndk
{
thread_func_return_t task_base::svc_run(void *args)
{
  if (args == 0) return 0;
  task_base* t = (task_base *)args;
  thread_manager *thr_mgr_ptr = t->thr_mgr();
  if (thr_mgr_ptr)
  {
    thr_mgr_ptr->set_thr_state(thread::self(), thread_manager::thr_running);
  }
  int svc_status = t->svc();
  thread_func_return_t exit_status;
  exit_status = reinterpret_cast<thread_func_return_t> (svc_status);

  t->cleanup();
  return exit_status;   // maybe used for pthread_join
}
int task_base::activate(int flags/* = thread::thr_join*/,
                        size_t n_threads/* = 1*/,
                        thread_t thread_ids[]/* = 0*/,
                        hthread_t thread_handles[]/* = 0*/,
                        int append_thr/* = 0*/,
                        size_t stack_size/* = 0*/,
                        int grp_id/* = -1*/)
{
  guard<thread_mutex> g(this->lock_); // avoid mutithread reentry
  if (this->thr_count_ > 0 && append_thr == 0)
    return 1;
  else
  {
    if (this->thr_count_ > 0 && this->grp_id_ != -1)
      grp_id = this->grp_id_;
  }
  //
  if (this->thr_mgr_ == 0)
    this->thr_mgr_ = thread_manager::instance();

  int grp_spawned = -1;
  grp_spawned = this->thr_mgr_->spawn_n(n_threads,
                                        &task_base::svc_run,
                                        (void*)this,
                                        thread_ids,
                                        flags,
                                        thread_handles,
                                        grp_id,
                                        stack_size,
                                        this);
  if (grp_spawned == -1)
    return -1;
  this->thr_count_ += n_threads;
  return 0;
}
} // namespace ndk

