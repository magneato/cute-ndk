namespace ndk
{
inline
reactor *reactor::instance(void)
{
  if (reactor::reactor_ == 0)
  {
    guard<thread_mutex> g(reactor::instance_lock_);
    if (reactor::reactor_ == 0) 
      reactor::reactor_ = new reactor();
  }
  return reactor::reactor_;
}
inline
reactor *reactor::instance(reactor *r)
{
  guard<thread_mutex> g(reactor::instance_lock_);
  reactor *t = reactor::reactor_;
  reactor::reactor_ = r;
  return t;
}
inline
void reactor::close_singleton(int del_reactor/* = 1*/)
{
  if (del_reactor)
  {
    delete reactor::reactor_;
  }
  reactor::reactor_ = 0;
}
inline
int reactor::handle_events(const time_value *max_wait_time/* = 0*/)
{
  return this->reactor_impl_->handle_events(max_wait_time);
}
inline
int reactor::run_reactor_event_loop(void)
{
  while (1)
  {
    if (this->reactor_impl_->handle_events() == -1)
      break;
  }
  return -1;
}
inline
int reactor::register_handler(ndk_handle handle,
                              event_handler *eh,
                              reactor_mask mask)
{
  reactor *old_r = eh->get_reactor();
  eh->set_reactor(this);
  int result = this->reactor_impl_->register_handler(handle,
                                                     eh,
                                                     mask);
  if (result == -1)
    eh->set_reactor(old_r);
  return result;
}
inline
int reactor::register_handler(event_handler *eh,
                              reactor_mask mask)
{
  reactor *old_r = eh->get_reactor();
  eh->set_reactor(this);
  int result = this->reactor_impl_->register_handler(eh,
                                                     mask);
  if (result == -1)
    eh->set_reactor(old_r);
  return result;
}
inline
int reactor::remove_handler(ndk_handle handle, reactor_mask mask)
{
  return this->reactor_impl_->remove_handler(handle, mask);
}
inline
int reactor::remove_handler(event_handler *eh,
                            reactor_mask mask)
{
  return this->reactor_impl_->remove_handler(eh, mask);
}
inline
int reactor::schedule_timer(event_handler *eh,
                            const void *arg,
                            const time_value &delay,
                            const time_value &interval/* = time_value::zero*/)
{
  return this->reactor_impl_->schedule_timer(eh,
                                             arg,
                                             delay,
                                             interval);
}
inline
int reactor::crontab(const event_handler *eh, 
                     const void *arg, 
                     const char *entry)
{
  return this->reactor_impl_->crontab(eh, arg, entry);
}
inline
int reactor::reset_crontab(int timer_id, const char *entry)
{
  return this->reactor_impl_->reset_crontab(timer_id, entry);
}
inline
int reactor::reset_timer_interval(int timer_id,
                                  const time_value &interval)
{
  return this->reactor_impl_->reset_timer_interval(timer_id,
                                                   interval);
}
inline
int reactor::cancel_timer(int timer_id,
                          const void **arg/* = 0*/,
                          int dont_call_handle_close/* = 1*/)
{
  return this->reactor_impl_->cancel_timer(timer_id,
                                           arg,
                                           dont_call_handle_close);
}
inline
int reactor::cancel_timer(event_handler *eh,
                          int dont_call_handle_close/* = 1*/)
{
  return this->reactor_impl_->cancel_timer(eh,
                                           dont_call_handle_close);
}
inline
int reactor::notify(event_handler *eh/* = 0*/,
                    reactor_mask mask/* = event_handler::except_mask*/)
{
  return this->reactor_impl_->notify(eh, mask);
}
inline
int reactor::purge_pending_notifications(event_handler *eh/* = 0*/,
                                         reactor_mask mask/* = event_handler::all_events_mask*/)
{
  return this->reactor_impl_->purge_pending_notifications(eh, mask);
}
inline
token &reactor::lock()
{
  return this->reactor_impl_->lock();
}
inline
int reactor::close()
{
  return this->reactor_impl_->close();
}
} // namespace ndk
