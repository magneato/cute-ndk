#include "ndk/unix_reactor.h"

#ifdef NDK_RTLOG
# include <cstring>
# include <cstdio>
#endif

namespace ndk
{
int unix_reactor_handler_repository::open(int size)
{
  STRACE("");
  if (size <= 0) return -1;
  this->handlers_   = new reactor_event_tuple[size];
  if (this->handlers_ == 0)
    return -1;
  this->max_size_ = size;
  // Try to set the number of handles to the current limit. 
  // Deny user to spawn new handle that overtop of the size, it'll overflow
  return 0;
}
int unix_reactor_handler_repository::close()
{
  STRACE("");
  if (this->handlers_ != 0)
  {
    this->unbind_all();
    delete []this->handlers_;
    this->handlers_  = 0; 
    this->max_size_  = 0;
  }
  return 0;    
}
int unix_reactor_handler_repository::unbind_all()
{
  STRACE("");
  for (int handle = 0; handle < this->max_size_; ++handle)
    this->unbind(handle);
  return 0;
}
//---------------------------------------------------------------------------
unix_reactor_notify::unix_reactor_notify()
: reactor_impl_(0),
  notification_pipe_()
{
}
int unix_reactor_notify::open(void *impl)
{
  STRACE("");
  this->reactor_impl_ = (unix_reactor *)impl;
  if (this->reactor_impl_ == 0) return -1;
  if (this->notification_pipe_.open() == -1) return -1;
  // 
  // close-on-exec
  ::fcntl(this->notification_pipe_.read_handle(), F_SETFD, FD_CLOEXEC);
  ::fcntl(this->notification_pipe_.write_handle(), F_SETFD, FD_CLOEXEC);

  ndk::set_non_block_mode(this->notification_pipe_.read_handle());
  ndk::set_non_block_mode(this->notification_pipe_.write_handle());
  return 0;
}
int unix_reactor_notify::close()
{
  STRACE("");
  return this->notification_pipe_.close();
}
int unix_reactor_notify::notify(event_handler *eh/* = 0*/,
                                 reactor_mask mask/* = event_handler::except_mask*/)
{
  STRACE("");
  if (this->reactor_impl_ == 0) return -1;

  // 
  notification_buffer buff(eh, mask);
  return ndk::write(this->notification_pipe_.write_handle(), 
                    (void *)&buff, 
                    sizeof(buff)) > 0 ? 0 : -1;
}
int unix_reactor_notify::purge_pending_notifications(event_handler * /*eh = 0*/,
                                                      reactor_mask/* = event_handler::all_events_mask*/)
{
  STRACE("");
  return -1;
}
// ------------------------------------------------------------------------
unix_reactor::unix_reactor()
: size_(0),
  delete_timer_queue_(0),
  timer_queue_(),
  notify_handler_(0)
{
}
int unix_reactor::open_ii(int size/* = 0*/, timer_queue *tq/* = 0*/) 
{
  STRACE("");
  this->timer_queue_ = tq;
  this->size_        = size;

  int result = 0;
  this->notify_handler_ = new unix_reactor_notify();
  if (this->notify_handler_ == 0)
  {
    this->close_ii(); return -1;
  }

  if (this->handler_rep_.open(this->size_) != 0)
  {
    this->close_ii(); return -1;
  }
  if (this->timer_queue_ == 0)
  {
    this->timer_queue_ = new timer_queue();
    if (this->timer_queue_ == 0)
    {
      this->close_ii(); return -1;
    }else this->delete_timer_queue_ = 1;
  }

  if (this->notify_handler_->open(this) == -1
      || 
      this->register_handler_i(this->notify_handler_->notify_handle(),
                               this->notify_handler_,
                               event_handler::read_mask) == -1)
    result = -1;

  if (result == -1) this->close_ii();

  return result;
}
int unix_reactor::close_ii(void) 
{
  STRACE("");
  if (this->timer_queue_)
    delete this->timer_queue_;
  this->timer_queue_ = 0;
  if (this->notify_handler_)
    delete this->notify_handler_;
  this->notify_handler_ = 0;

  this->handler_rep_.close();
  return 0;
}
int unix_reactor::poll_events_i(const time_value *max_wait_time)
{
  STRACE("");
  time_value wait_time(0, 0);
  time_value *this_timeout = 0;
  this_timeout = this->timer_queue_->calculate_timeout(max_wait_time, 
                                                       &wait_time);

  // Check if we have timers to fire.
  const int timers_pending =
    ((this_timeout != 0 && max_wait_time == 0)  // one timer
     || (this_timeout != 0 && max_wait_time != 0
         && *this_timeout != *max_wait_time) ? 1 : 0); 

  int nfds = this->poll_events(this_timeout);
  // If timers are pending, override any timeout from the epoll.
  return((nfds == 0 && timers_pending != 0) ? 1 : nfds);
}
int unix_reactor::handle_events_i(const time_value *max_wait_time) 
{
  STRACE("");
  int result  = 0;
  do
  {
    result = this->poll_events_i(max_wait_time);
  }while (result == -1 && errno == EINTR);

  // timeout
  if (result == 0)
    return 0;
  else if (result == -1 && errno != EINTR)
  {
#ifdef NDK_RTLOG
    fprintf(stderr, "warning: %s\n", strerror(errno));
#endif
    return result;
  }
  // Dispatch an event.
  return this->dispatch_events();
}
int unix_reactor::dispatch_events()
{
  STRACE("");
  int result = 0;

  // Handle timers early since they may have higher latency
  // constraints than I/O handlers.  Ideally, the order of
  // dispatching should be a strategy...

  result += this->dispatch_timer_handler();

  // Check to see if there are no more I/O handles left to
  // dispatch AFTER we've handled the timers.
  //
  result += this->dispatch_io_events();

  return result;
}
int unix_reactor::register_handler_i(ndk_handle handle,
                                     event_handler *eh,
                                     reactor_mask mask)
{
  STRACE("");
  if (this->handler_rep_.find(handle) == 0)
  {
    // Handler not present in the repository.  Bind it.
    if (this->handler_rep_.bind(handle, eh, mask) != 0)
      return -1;
    if (this->handle_opt_i(handle, mask, unix_reactor::add_mask) != 0)
    {
      this->handler_rep_.unbind(handle);
      return -1;
    }
  }else
  {
    // Handler is already present in the repository, so register it
    // again, possibly for different event.  Add new mask to the
    // current one
    reactor_mask new_mask = this->handler_rep_.mask(handle);
    NDK_SET_BITS(new_mask, mask);
    this->handler_rep_.mask(handle, new_mask);
    return this->handle_opt_i(handle, new_mask, unix_reactor::set_mask);
  }
  return 0;
}
int unix_reactor::remove_handler_i(ndk_handle handle,
                                   reactor_mask mask)
{
  STRACE("");
  event_handler *eh = this->handler_rep_.find(handle);
  if (eh == 0) return -1;

  if (NDK_BIT_DISABLED(mask, event_handler::dont_call))
  {
    eh->handle_close(handle, mask);
    if (this->handler_rep_.find(handle) == 0)
      return 0;
  }

  reactor_mask new_mask = this->handler_rep_.mask(handle);
  NDK_CLR_BITS(new_mask, mask);
  this->handler_rep_.mask(handle, new_mask);

  if (this->handle_opt_i(handle, new_mask, unix_reactor::clr_mask) == -1)
    return -1;

  // If there are no longer any outstanding events on the given handle
  // then remove it from the handler repository.
  if (new_mask == event_handler::null_mask)
    this->handler_rep_.unbind(handle);

  return 0;
}
} // namespace ndk

