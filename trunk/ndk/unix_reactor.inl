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
// --------------------------------------------------------------------------
inline
int unix_reactor_notify::read_notify_pipe(ndk_handle ,
                                          notification_buffer &buffer)
{
  return ndk::read(this->notification_pipe_.read_handle(),
                   (char *)&buffer,
                   sizeof(buffer));
}
inline
int unix_reactor_notify::handle_input(ndk_handle handle)
{
  STRACE("");
  notification_buffer buffer;
  this->read_notify_pipe(handle, buffer);
  return 0;
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
