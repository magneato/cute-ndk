namespace ndk
{
inline
int unix_reactor_handler_repository::unbind(ndk_handle handle)
{
  STRACE("");
  if (this->find(handle) == 0) return -1;
  this->handlers_[handle].event_handler_ = 0;
  this->handlers_[handle].mask_ = event_handler::null_mask;
  return 0;
}
inline
event_handler *unix_reactor_handler_repository::find(ndk_handle handle,
                                                     size_t *index_p/* = 0*/)
{
  STRACE("");
  event_handler *eh = 0;
  if (this->handle_in_range(handle))
  {
    eh = this->handlers_[handle].event_handler_;
    if (eh && index_p) *index_p = handle;
  }
  return eh;
}
inline
int unix_reactor_handler_repository::bind(ndk_handle handle,
                                          event_handler *eh,
                                          reactor_mask mask)
{
  STRACE("");
  if (eh == 0 || this->handlers_ == 0) return -1;
  if (this->is_invalid_handle(handle)) return -1;
  this->handlers_[handle].event_handler_ = eh;
  this->handlers_[handle].mask_ = mask;
  return 0;
}
inline
reactor_mask unix_reactor_handler_repository::mask(ndk_handle handle)
{
  if (this->handle_in_range (handle))
    return this->handlers_[handle].mask_;
  return event_handler::null_mask;
}
inline
void unix_reactor_handler_repository::mask(ndk_handle handle,
                                           reactor_mask mask)
{
  if (this->handle_in_range(handle))
    this->handlers_[handle].mask_ = mask;
}
inline
int unix_reactor_handler_repository::handle_in_range(ndk_handle handle) const
{
  if (handle >= 0 && handle < this->max_size_) return 1;
  return 0;
}
inline
int unix_reactor_handler_repository::is_invalid_handle(ndk_handle handle) const
{
  if (handle < 0 || handle >= this->max_size_)
    return 1;
  return 0;
}
inline
void unix_reactor_handler_repository::suspend(ndk_handle handle)
{
  if (this->handle_in_range(handle))
    this->handlers_[handle].suspended_ = true;
}
inline
void unix_reactor_handler_repository::resume(ndk_handle handle)
{
  if (this->handle_in_range(handle))
    this->handlers_[handle].suspended_ = false;
}
inline
bool unix_reactor_handler_repository::suspended(ndk_handle handle) const
{
  if (this->handle_in_range(handle))
    return this->handlers_[handle].suspended_;
  return false;
}
// --------------------------------------------------------------------------
inline
unix_reactor_notify_tuple *unix_reactor_notify::alloc_notify_tuple(event_handler *eh,
                                                                   void *msg)
{
  STRACE("");
  unix_reactor_notify_tuple *tuple = 0;
  if (this->free_notify_msg_queue_ == 0)
    this->free_notify_msg_queue_ = new unix_reactor_notify_tuple(eh, msg);
  else
  {
    this->free_notify_msg_queue_->event_handler_ = eh;
    this->free_notify_msg_queue_->msg_ = msg;
  }
  tuple = this->free_notify_msg_queue_;
  this->free_notify_msg_queue_ = this->free_notify_msg_queue_->next();
  tuple->next(0);
  return tuple;
}
inline
void unix_reactor_notify::release_notify_tuple(unix_reactor_notify_tuple *tuple)
{
  STRACE("");
  if (tuple == 0) return ;
  tuple->next(this->free_notify_msg_queue_);
  this->free_notify_msg_queue_ = tuple;
}
inline
void unix_reactor_notify::push_notify_tuple(unix_reactor_notify_tuple *tuple)
{
  STRACE("");
  if (this->notify_msg_queue_tail_ == 0)
    this->notify_msg_queue_ = tuple;
  else
    this->notify_msg_queue_tail_->next(tuple);
  this->notify_msg_queue_tail_ = tuple;
}
inline
unix_reactor_notify_tuple *unix_reactor_notify::pop_notify_tuple()
{
  STRACE("");
  unix_reactor_notify_tuple *tuple = 0;
  if (this->notify_msg_queue_)
  {
    tuple = this->notify_msg_queue_;
    this->notify_msg_queue_ = this->notify_msg_queue_->next();
    if (this->notify_msg_queue_ == 0)
      this->notify_msg_queue_tail_ = 0;
  }
  return tuple;
}
inline
int unix_reactor_notify::read_notify_msg(unix_reactor_notify_tuple *free_tuple,
                                         unix_reactor_notify_tuple *&tuple)
{
  STRACE("");
  guard<notify_mtx> g(this->notify_mutex_);
  this->release_notify_tuple(free_tuple);
  char a;
  int result = ndk::read(this->notification_pipe_.read_handle(),
                         &a,
                         1);
  if (result == 1)
  {
    tuple = this->pop_notify_tuple();
    assert(tuple != 0);
    return 0;
  }
  return -1;
}
inline
ndk_handle unix_reactor_notify::notify_handle()
{
  return this->notification_pipe_.read_handle();
}
// ------------------------------------------------------------------------
inline
int unix_reactor::dispatch_timer_handler()
{
  STRACE("");
  return this->timer_queue_->expire(time_value::gettimeofday());
}
inline
int unix_reactor::upcall(event_handler *eh,
                         int (event_handler::*callback)(ndk_handle),
                         ndk_handle handle)
{
  return (eh->*callback)(handle);
}
} // namespace ndk
