#include "ndk/multi_reactor.h"

namespace ndk
{
template<typename REACTOR>
multi_reactor_handle_repository<REACTOR>::open(int max_size)
{
#if defined(NDK_LINUX)
  assert(this->reactor_handle_map_ == 0);
  this->reactor_handle_map_ = new REACTOR *[max_size];
  if (this->reactor_handle_map_ == 0)
    return -1;
#endif
  return 0;
}
template<typename REACTOR>
multi_reactor<REACTOR>::multi_reactor()
: strategy_(reactor_num_fixed),
  reactor_count_(4),
  max_reactor_count_(64),
  max_handles_(0),
  reactor_index_(0),
  single_reactor_capacity_(0),
  reactors_(0)
{
}
template<typename REACTOR>
multi_reactor<REACTOR>::~multi_reactor()
{
  STRACE("multi_reactor");
  this->close();
}
template<typename REACTOR>
int multi_reactor<REACTOR>::init(int strategy/* = reactor_num_fixed*/,
                                 int count,
                                 int max_handlers/* = 0*/)
{
  STRACE("multi_reactor");
  // check value
  if (count <= 0) return -1;  // avoid n/0
  // 
  this->max_handles_ = max_handlers;
  if (this->max_handles_ == 0)
  {
    this->max_handles_ = NDK::max_handles();
    if (this->max_handles_ <= 0)
    {
      this->close();
      return -1;
    }
  }else if (NDK::set_handle_limit(this->max_handles_) != 0)
    return -1;
  this->reactor_handle_map_ = new multi_reactor_handle_repository;
  if (this->reactor_handle_map_->open(this->max_handles_) != 0)
    return -1;
  this->strategy_ = strategy;
  if (this->strategy_ == reactor_num_fixed)
  {
    this->reactor_count_ = count;
    if (this->reactor_count_ <= 0) 
      return -1;
    this->reactors_  = new REACTOR *[this->reactor_count_];
    for (int i = 0; i < this->reactor_count_; ++i)
      this->reactors_[i] = 0;

    this->single_reactor_capacity_ 
      = this->max_handles_ / this->reactor_count_;
    int over = this->max_handles_ % this->reactor_count_;

    for (int i = 0; i < this->reactor_count_; ++i)
    {
      this->reactors_[i] = new REACTOR(0);
      // luanch each reactor
      if (this->reactors_[i])
      {
        if (this->reactors_[i]->open(i == 0
                                     ? this->single_reactor_capacity_ + over
                                     : this->single_reactor_capacity_, 
                                     0, 
                                     false) != 0
            ||
            thread_manager::instance()->spawn(NDK::thr_run_reactor_event_loop, 
                                              this->reactors_[i]) == -1)
        {
          this->close();
          return -1;
        }
      }else
      {
        this->close();
        return -1;
      }
    }
  }else if (this->strategy_ == single_reactor_capacity_fixed)
  {
    this->single_reactor_capacity_ = count; // !!! Note 1/0
    this->reactor_count_ = 1;
    this->reactor_index_ = 0;
    this->max_reactor_count_ = 
      this->max_handles_ / this->single_reactor_capacity_;
    int over = this->max_handles_ % this->single_reactor_capacity_;
    if (over > 0)
      this->max_reactor_count_++;
    this->reactors_    = new REACTOR* [this->max_reactor_count_];
    this->reactors_[this->reactor_index_] = new REACTOR(0);
    if (this->reactors_[this->reactor_index_]->open(
                                                    this->single_reactor_capacity_, 
                                                    0, 
                                                    false) != 0
        ||
        thread_manager::instance()->spawn(
                                          NDK::thr_run_reactor_event_loop,
                                          this->reactors_[this->reactor_index_]) == -1)
    {
      this->close();
      return -1;
    }
  }else
    return -1;
  return 0; 
}
template<typename REACTOR>
int multi_reactor<REACTOR>::close()
{
  STRACE("multi_reactor");
  guard<thread_mutex> g(this->mutex_);
  for (int i = 0; i < this->reactor_count_; ++i)
  {
    if (this->reactors_[i])
      delete this->reactors_[i];
  }
  delete [] this->reactors_;

  if (this->reactors_repository_)
    delete this->reactors_repository_;
  this->reactors_repository_ = 0;
  this->reactors_ = 0;
  this->single_reactor_capacity_ = 0;
  this->reactor_count_ = 0;
  this->reactor_index_ = 0;
  this->max_reactor_count_ = 0;
  return 0;
}
template<typename REACTOR>
int multi_reactor<REACTOR>::schedule_one_reactor(void)
{
  if (this->reactors_ == 0) return 0;
  // 1. reactor_num_fixed
  if (this->strategy_ == reactor_num_fixed)
  {
    int index = 0;
    int min_payload = this->reactors_[index]->curr_payload();
    int payload = 0;
    for (int i = index + 1; i < this->reactor_count_; ++i)
    {
      payload = this->reactors_[i]->curr_payload();
      if (payload <= min_payload)
      {	
        min_payload = payload;
        index = i;
      }	
    }
    assert(index < this->reactor_count_);
    return this->reactors_[index];
  }else if (this->strategy_ == single_reactor_capacity_fixed)
    // 2. single_reactor_capacity_fixed	
  {
    if (this->reactors_[this->reactor_index_]->curr_payload() 
        >= this->single_reactor_capacity_)
    {
      int result = this->append_reactor();
      if (result != 0)
      {
        /**
         * If the number of Reactors is out of max_reactor_count
         * , then can't append reactor, but it should be poll other
         * reactors are out of its payload or not.
         */
        if (result == -2) // out of limit then poll back
        {
          int index = 0;
          for (; index < this->reactor_count_; ++index)
          {
            if (this->reactors_[index]->curr_payload() 
                < this->single_reactor_capacity_)
            {
              this->reactor_index_ = index;
              break;
            }
          }
          if (index == this->reactor_count_)
            return 0;
        }else
          return 0;
      }
    }
    return this->reactors_[this->reactor_index_];
  }
  return 0;
}
template<typename REACTOR>
int multi_reactor<REACTOR>::register_handler_i(ndk_handle handle,
                                               event_handler *event_handler,
                                               reactor_mask mask)
{
  STRACE("multi_reactor");
  REACTOR *r = this->reactors_repository_.find(handle);
  if (r == 0)
    r = this->spawn_reactor();
  if (r == 0) return -1;
  if (r->register_handler(handle, event_handler, mask) == 0)
  {
    if (this->reactors_repository_.bind(hanlde, r) == 0)
      return 0;
    else
      r->remove_handler(handle, mask);
  }
  return -1;
}
template<typename REACTOR>
int multi_reactor<REACTOR>::append_reactor()
{
  STRACE("multi_reactor");
  if (this->reactors_)
  {
    int index = this->reactor_index_ + 1;
    if (index >= this->max_reactor_count_)
      return -2;
    this->reactors_[index] = new REACTOR(0);
    if (this->reactors_[index] 
        && this->reactors_[index]->open(this->single_reactor_capacity_, 
                                        0,
                                        false) == 0
        && thread_manager::instance()->spawn(
                                             ndk::thr_run_reactor_event_loop,
                                             this->reactors_[index]) != -1)
    {
      ++this->reactor_count_;
      ++this->reactor_index_;
      return 0;
    }
  }
  return -1;
}
template<typename REACTOR>
int multi_reactor<REACTOR>::register_handler(ndk_handle handle,
                                             event_handler *event_handler,
                                             reactor_mask mask)
{
  STRACE("multi_reactor");
  guard<thread_mutex> g(this->mutex_);
  return this->register_handler_i(handle,
                                  event_handler,
                                  mask);
}
template<typename REACTOR>
int multi_reactor<REACTOR>::register_handler(event_handler *event_handler,
                                             reactor_mask mask)
{
  STRACE("multi_reactor");
  guard<thread_mutex> g(this->mutex_);
  return this->register_handler_i(event_handler->handle(),
                                  event_handler,
                                  mask);
}
template<typename REACTOR>
int multi_reactor<REACTOR>::remove_handler(event_handler *event_handler,
                                           reactor_mask mask)
{
  STRACE("multi_reactor");
  guard<thread_mutex> g(this->mutex_);
  return this->remove_handler_i(event_handler->handle(),
                                mask);
}
template<typename REACTOR>
int multi_reactor<REACTOR>::remove_handler(ndk_handle handle,
                                           reactor_mask mask)
{
  STRACE("multi_reactor");
  guard<thread_mutex> g(this->mutex_);
  return this->remove_handler_i(handle, mask);
}
template<typename REACTOR>
int multi_reactor<REACTOR>::remove_handler_i(ndk_handle handle,
                                             reactor_mask mask)
{
  STRACE("multi_reactor");
  REACTOR *r = this->reactors_repository_->find(handle);
  if (r == 0)
    return -1;
  if (r->remove_handler(handle, mask) == 0)
  {
    this->notify_remove_handle_i(handle, mask);
    return 0;
  }
  return -1;
}
template<typename REACTOR>
void multi_reactor<REACTOR>::notify_remove_handle(ndk_handle handle,
                                                  reactor_mask mask)
{
  guard<thread_mutex> g(this->mutex_);
  this->notify_remove_handle_i(handle, mask);
}
template<typename REACTOR>
void multi_reactor<REACTOR>::notify_remove_handle_i(ndk_handle handle,
                                                    reactor_mask mask)
{
  reactor_mask new_mask = this->reactors_repository_->mask(handle);
  NDK_CLR_BITS(new_mask, mask);
  if (new_mask == event_handler::null_mask)
  {
    this->reactors_repository_->unbind(handle);
  }else
  {
    this->reactors_repository_->mask(handle, new_mask);
  }
}
template<typename REACTOR>
int multi_reactor<REACTOR>::schedule_timer(event_handler *event_handler,
                                           const void *arg,
                                           const time_value &delay,
                                           const time_value &interval/* = time_value::zero*/)
{
  STRACE("multi_reactor");
  return 0;
}
template<typename REACTOR>
int multi_reactor<REACTOR>::reset_timer_interval(int timer_id,
                                                 const time_value &interval)
{
  STRACE("multi_reactor");
  return 0;
}
template<typename REACTOR>
int multi_reactor<REACTOR>::cancel_timer(int timer_id,
                                         const void **arg/* = 0*/,
                                         int dont_call_handle_close/* = 1*/)
{
  STRACE("multi_reactor");
  return 0;
}
template<typename REACTOR>
int multi_reactor<REACTOR>::cancel_timer(event_handler *eh,
                                         int dont_call_handle_close/* = 1*/)
{
  STRACE("multi_reactor");
  return 0;
}
template<typename REACTOR>
void multi_reactor<REACTOR>::stop_reactors(int count/* = -1*/)
{
  STRACE("multi_reactor");
}
template<typename REACTOR>
int multi_reactor<REACTOR>::notify(event_handler *eh/* = 0*/,
                                   reactor_mask mask/* = event_handler::EXCEPT_MASK*/,
                                   const time_value *timeout/* = 0*/)
{
  STRACE("multi_reactor");
  return 0;
}
template<typename REACTOR>
int multi_reactor<REACTOR>::handle_events(const time_value *max_wait_time/* = 0*/)
{
  STRACE("multi_reactor");
  return 0;
}
template<typename REACTOR>
int multi_reactor<REACTOR>::deactivated()
{
  STRACE("multi_reactor");
  return 0;
}
template<typename REACTOR>
void multi_reactor<REACTOR>::deactivate(int )
{
  STRACE("multi_reactor");
}
template<typename REACTOR>
size_t multi_reactor<REACTOR>::size() const
{
  STRACE("multi_reactor");
  return this->max_handles_;
}
template<typename REACTOR>
size_t multi_reactor<REACTOR>::curr_payload()
{
  STRACE("multi_reactor");
  return 0;
}
template<typename REACTOR>
thread_t multi_reactor<REACTOR>::owner()
{
  STRACE("multi_reactor");
  return 0;
}
template<typename REACTOR>
void multi_reactor<REACTOR>::owner(thread_t )
{
  STRACE("multi_reactor");
  return ;
}
} // namespace ndk

