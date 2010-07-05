namespace ndk
{
inline
int epoll_reactor<reactor_token>::open(timer_queue *tq/* = 0*/)
{
  guard<reactor_token> g(this->token_);
  return epoll_reactor_base::open_i(tq);
}
inline int epoll_reactor<reactor_token>::close()
{
  guard<reactor_token> g(this->token_);
  return epoll_reactor_base::close_i();
}
inline
int epoll_reactor<reactor_token>::handle_events(const time_value *max_wait_time)
{
  STRACE("");
  token_guard g(this->token_);
  int result = g.acquire_quietly(max_wait_time);
  if (!g.is_owner()) return result;

  return this->handle_events_i(max_wait_time);
}
inline
int epoll_reactor<reactor_token>::register_handler(event_handler *eh,
                                                   reactor_mask mask)
{
  STRACE("");
  if (eh == 0
      || eh->get_handle() == NDK_INVALID_HANDLE
      || mask == event_handler::null_mask)
    return -1;
  guard<reactor_token> g(this->token_);
  return this->register_handler_i(eh->get_handle(),
                                  eh,
                                  mask);
}
inline
int epoll_reactor<reactor_token>::register_handler(ndk_handle handle,
                                                   event_handler *eh,
                                                   reactor_mask mask)
{
  STRACE("");
  if (handle == NDK_INVALID_HANDLE
      || eh == 0
      || mask == event_handler::null_mask)
    return -1;
  guard<reactor_token> g(this->token_);
  return this->register_handler_i(handle,
                                  eh,
                                  mask);
}
inline
int epoll_reactor<reactor_token>::remove_handler(event_handler *eh,
                                                 reactor_mask mask)
{
  STRACE("");
  if (eh == 0
      || eh->get_handle() == NDK_INVALID_HANDLE
      || mask == event_handler::null_mask)
    return -1;
  guard<reactor_token> g(this->token_);
  return this->remove_handler_i(eh->get_handle(),
                                mask);
}
inline
int epoll_reactor<reactor_token>::remove_handler(ndk_handle handle,
                                                 reactor_mask mask)
{
  STRACE("");
  if (handle == NDK_INVALID_HANDLE
      || mask == event_handler::null_mask)
    return -1;
  guard<reactor_token> g(this->token_);
  return this->remove_handler_i(handle,
                                mask);
}
inline
int epoll_reactor<reactor_token>::suspend_handler(ndk_handle handle)
{
  STRACE("");
  if (handle == NDK_INVALID_HANDLE) return -1;
  guard<reactor_token> g(this->token_);
  return this->suspend_handler_i(handle);
}
inline
int epoll_reactor<reactor_token>::suspend_handler(event_handler *eh)
{
  STRACE("");
  if (eh == 0) return -1;
  guard<reactor_token> g(this->token_);
  return this->suspend_handler_i(eh->get_handle());
}
inline
int epoll_reactor<reactor_token>::resume_handler(ndk_handle handle)
{
  STRACE("");
  if (handle == NDK_INVALID_HANDLE) return -1;
  guard<reactor_token> g(this->token_);
  return this->resume_handler_i(handle);
}
inline
int epoll_reactor<reactor_token>::resume_handler(event_handler *eh)
{
  STRACE("");
  if (eh == 0) return -1;
  guard<reactor_token> g(this->token_);
  return this->resume_handler_i(eh->get_handle());
}
inline
int epoll_reactor<reactor_token>::schedule_timer(event_handler *eh,
                                                 const void *arg,
                                                 const time_value &delay,
                                                 const time_value &interval/* = time_value::zero*/)
{
  STRACE("");
  if (0 == this->timer_queue_ || eh == 0) return -1;

  guard<reactor_token> g(this->token_);
  return this->timer_queue_->schedule(eh,
                                      arg,
                                      delay,
                                      interval);
}
inline
int epoll_reactor<reactor_token>::crontab(const event_handler *eh,
                                          const void *arg,
                                          const char *entry)
{
  STRACE("");
  if (0 == this->timer_queue_ 
      || eh == 0
      || entry == 0) return -1;

  guard<reactor_token> g(this->token_);
  return this->timer_queue_->crontab(eh, arg, entry);
}
inline
int epoll_reactor<reactor_token>::reset_crontab(int timer_id,
                                                const char *entry)
{
  STRACE("");
  if (0 == this->timer_queue_ 
      || timer_id == -1 
      || entry == 0) return -1;

  guard<reactor_token> g(this->token_);
  return this->timer_queue_->reset_crontab(timer_id, entry);
}
inline
int epoll_reactor<reactor_token>::reset_timer_interval(int timer_id,
                                                       const time_value &interval)
{
  STRACE("");
  if (0 == this->timer_queue_ || timer_id < 0) return -1;

  guard<reactor_token> g(this->token_);
  return this->timer_queue_->reset_interval(timer_id, interval);
}
inline
int epoll_reactor<reactor_token>::cancel_timer(int timer_id,
                                               const void **arg/* = 0*/,
                                               int dont_call_handle_close/* = 1*/)
{
  STRACE("");
  if (0 == this->timer_queue_ || timer_id < 0) return -1;

  guard<reactor_token> g(this->token_);
  return this->timer_queue_->cancel(timer_id,
                                    arg,
                                    dont_call_handle_close);
}
inline
int epoll_reactor<reactor_token>::cancel_timer(event_handler *eh,
                                               int dont_call_handle_close/* = 1*/)
{
  STRACE("");
  if (0 == this->timer_queue_ || eh == 0) return -1;

  guard<reactor_token> g(this->token_);
  return this->timer_queue_->cancel(eh, dont_call_handle_close);
}
inline
token &epoll_reactor<reactor_token>::lock()
{
  return this->token_;
}
// ----------------------------------------------------------------------------
inline
int epoll_reactor<reactor_null_token>::open(timer_queue *tq/* = 0*/)
{
  return epoll_reactor_base::open_i(tq);
}
inline
int epoll_reactor<reactor_null_token>::close()
{
  return epoll_reactor_base::close_i();
}
inline
int epoll_reactor<reactor_null_token>::handle_events(const time_value *max_wait_time)
{
  STRACE("");
  return this->handle_events_i(max_wait_time);
}
inline
int epoll_reactor<reactor_null_token>::register_handler(event_handler *eh,
                                                        reactor_mask mask)
{
  STRACE("");
  if (eh == 0
      || eh->get_handle() == NDK_INVALID_HANDLE
      || mask == event_handler::null_mask)
    return -1;
  return this->register_handler_i(eh->get_handle(),
                                  eh,
                                  mask);
}
inline
int epoll_reactor<reactor_null_token>::register_handler(ndk_handle handle,
                                                        event_handler *eh,
                                                        reactor_mask mask)
{
  STRACE("");
  if (handle == NDK_INVALID_HANDLE
      || eh == 0
      || mask == event_handler::null_mask)
    return -1;
  return this->register_handler_i(handle,
                                  eh,
                                  mask);
}
inline
int epoll_reactor<reactor_null_token>::remove_handler(event_handler *eh,
                                                      reactor_mask mask)
{
  STRACE("");
  if (eh == 0
      || eh->get_handle() == NDK_INVALID_HANDLE
      || mask == event_handler::null_mask)
    return -1;
  return this->remove_handler_i(eh->get_handle(),
                                mask);
}
inline
int epoll_reactor<reactor_null_token>::remove_handler(ndk_handle handle,
                                                      reactor_mask mask)
{
  STRACE("");
  if (handle == NDK_INVALID_HANDLE
      || mask == event_handler::null_mask)
    return -1;
  return this->remove_handler_i(handle,
                                mask);
}
inline
int epoll_reactor<reactor_null_token>::suspend_handler(ndk_handle handle)
{
  STRACE("");
  if (handle == NDK_INVALID_HANDLE) return -1;
  return this->suspend_handler_i(handle);
}
inline
int epoll_reactor<reactor_null_token>::suspend_handler(event_handler *eh)
{
  STRACE("");
  if (eh == 0) return -1;
  return this->suspend_handler_i(eh->get_handle());
}
inline
int epoll_reactor<reactor_null_token>::resume_handler(ndk_handle handle)
{
  STRACE("");
  if (handle == NDK_INVALID_HANDLE) return -1;
  return this->resume_handler_i(handle);
}
inline
int epoll_reactor<reactor_null_token>::resume_handler(event_handler *eh)
{
  STRACE("");
  if (eh == 0) return -1;
  return this->resume_handler_i(eh->get_handle());
}
inline
int epoll_reactor<reactor_null_token>::schedule_timer(event_handler *eh,
                                                      const void *arg,
                                                      const time_value &delay,
                                                      const time_value &interval/* = time_value::zero*/)
{
  STRACE("");
  if (0 == this->timer_queue_ || eh == 0) return -1;
  return this->timer_queue_->schedule(eh,
                                      arg,
                                      delay,
                                      interval);
}
inline
int epoll_reactor<reactor_null_token>::crontab(const event_handler *eh,
                                               const void *arg,
                                               const char *entry)
{
  STRACE("");
  if (0 == this->timer_queue_ 
      || eh == 0
      || entry == 0) return -1;
  return this->timer_queue_->crontab(eh, arg, entry);
}
inline
int epoll_reactor<reactor_null_token>::reset_crontab(int timer_id,
                                                     const char *entry)
{
  STRACE("");
  if (0 == this->timer_queue_ 
      || timer_id == -1 
      || entry == 0) return -1;

  return this->timer_queue_->reset_crontab(timer_id, entry);
}
inline
int epoll_reactor<reactor_null_token>::reset_timer_interval(int timer_id,
                                                            const time_value &interval)
{
  STRACE("");
  if (0 == this->timer_queue_ || timer_id < 0) return -1;
  return this->timer_queue_->reset_interval(timer_id, interval);
}
inline
int epoll_reactor<reactor_null_token>::cancel_timer(int timer_id,
                                                    const void **arg/* = 0*/,
                                                    int dont_call_handle_close/* = 1*/)
{
  STRACE("");
  if (0 == this->timer_queue_ || timer_id < 0) return -1;
  return this->timer_queue_->cancel(timer_id,
                                    arg,
                                    dont_call_handle_close);
}
inline
int epoll_reactor<reactor_null_token>::cancel_timer(event_handler *eh,
                                                    int dont_call_handle_close/* = 1*/)
{
  STRACE("");
  if (0 == this->timer_queue_ || eh == 0) return -1;
  return this->timer_queue_->cancel(eh, dont_call_handle_close);
}
inline
token &epoll_reactor<reactor_null_token>::lock()
{
  return this->token_;
}
} // namespace ndk
