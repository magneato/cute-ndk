#include "ndk/kqueue_reactor.h"

#include <cstring>
#include <cstdio>

namespace ndk
{
kqueue_reactor_base::kqueue_reactor_base()
: unix_reactor(),
  initialized_(false),
  kqueue_fd_(NDK_INVALID_HANDLE),
  events_(0),
  start_pevents_(0),
  end_pevents_(0)
{
}
int kqueue_reactor_base::open_i(timer_queue *tq)
{
  if (this->initialized_) return -1;

  int size = ndk::max_handles();
  if (size <= 0) return -1;
  this->events_ = new struct kevent[size];
  if (this->events_ == 0)
    return -1;

  // Since Linux 2.6.8, the size argument is unused.
  this->kqueue_fd_ = ::kqueue();
  if (this->kqueue_fd_ == NDK_INVALID_HANDLE)
  {
    this->close_i(); return -1;
  }
  ::fcntl(this->kqueue_fd_, F_SETFD, FD_CLOEXEC);

  if (unix_reactor::open_ii(size, tq) != 0)
  {
    this->close_i(); return -1;
  }
  this->initialized_ = true;
  return 0;
}
int kqueue_reactor_base::close_i()
{
  if (this->kqueue_fd_ != NDK_INVALID_HANDLE)
    ::close(this->kqueue_fd_);
  this->kqueue_fd_ = NDK_INVALID_HANDLE;
  if (this->events_)
    delete [] this->events_;
  this->events_ = 0;          
  this->initialized_   = false;
  return unix_reactor::close_ii();
}
int kqueue_reactor_base::poll_events(const time_value *timeout)
{
  if (this->nevents_ > 0) return 1;

  struct timespec *pts = 0;
  struct timespec ts;
  if (timeout != 0)
  {
    ts = (*timeout);
    pts = &ts;
  }
  int nfds = ::kevent(this->kqueue_fd_, 
                      0,
                      0,
                      this->events_,
                      this->size_,
                      pts);
  if (nfds > 0)
    this->nevents_ = nfds;
  return nfds;
}
int kqueue_reactor_base::dispatch_io_events()
{
  STRACE("");
  struct kevent *& pfd = this->events_;
  int result = 0;
  for (int i = 0;
       this->nevents_ > 0 && result < MAX_REACTOR_PROCESS_FDS_ONE_TIME;
       )
  {
    // 
    event_handler *eh = this->handler_rep_.find(this->events_[i].ident);
    if (eh)
    {
      ++result;
      if (NDK_BIT_ENABLED(this->events_[i].flags, EV_ERROR))
      {
        this->remove_handler_i(this->events_[i].ident, 
                               event_handler::all_events_mask);
      }else if (NDK_BIT_ENABLED(this->events_[i].filter, EVFILT_READ))
      {
        NDK_CLR_BITS(this->events_[i].filter, EVFILT_READ);
        if (this->upcall(eh, 
                         &event_handler::handle_input, 
                         this->events_[i].ident) < 0)
          this->remove_handler_i(this->events_[i].ident, 
                                 event_handler::read_mask);
      }else if (NDK_BIT_ENABLED(this->events_[i].filter, EVFILT_WRITE))
      {
        NDK_CLR_BITS(this->events_[i].filter, EVFILT_WRITE);
        if (this->upcall(eh, 
                         &event_handler::handle_output, 
                         this->events_[i].ident) < 0)
          this->remove_handler_i(this->events_[i].ident, 
                                 event_handler::write_mask);
      }else
      {
        fprintf(stderr, 
                "dispatch_io [handle = %d] trigger unknown events 0x%x\n",
                this->events_[i].ident, 
                this->events_[i].filter);
        if (this->events_[i].filter != 0)
          ++i;
        --result;
      } 
      // If more than one event comes in between kevent calls,
      // they will be combined reported.
      if (this->events_[i].filter == 0)
        ++i; 
    }else // if (eh)
      ++i; 
  } // 
  return result;
}
int kqueue_reactor_base::reactor_mask_to_kqueue_event(reactor_mask mask)
{
  STRACE("");
  int events = 0;
  if (NDK_BIT_ENABLED(mask, event_handler::read_mask))
    NDK_SET_BITS(events, EVFILT_READ);

  if (NDK_BIT_ENABLED(mask, event_handler::accept_mask))
    NDK_SET_BITS(events, EVFILT_READ);

  if (NDK_BIT_ENABLED(mask, event_handler::connect_mask))
    NDK_SET_BITS(events, EVFILT_READ|EVFILT_WRITE);

  if (NDK_BIT_ENABLED(mask, event_handler::write_mask))
    NDK_SET_BITS(events, EVFILT_WRITE);

  return events;
}
int kqueue_reactor_base::handle_opt_i(ndk_handle handle,
                                     reactor_mask mask,
                                     int opt)
{
  STRACE("");
  struct kevent kev;
  int filter = 0;

  if (mask == event_handler::null_mask)
    opt = unix_reactor::clr_mask;
  else if (opt == unix_reactor::clr_mask)
    opt = unix_reactor::set_mask;

  switch(opt)
  {
  case unix_reactor::add_mask: 
    filter = this->reactor_mask_to_kqueue_event(mask);
    EV_SET(&kev, handle, filter, EV_ADD, 0, 0, 0);
    return ::kevent(this->kqueue_fd_, &kev, 1, 0, 0, 0);	
  case unix_reactor::set_mask: 
    filter = this->reactor_mask_to_kqueue_event(mask);
    EV_SET(&kev, handle, filter, EV_ADD, 0, 0, 0);
    if (::kqueue_ctl(this->kqueue_fd_, EPOLL_CTL_MOD, handle, &epev) == -1)
    {
      // If a handle is closed, kqueue removes it from the poll set
      // automatically - we may not know about it yet. If that's the
      // case, a mod operation will fail with ENOENT. Retry it as
      // an add.
      if (errno == ENOENT)
        return ::kqueue_ctl(this->kqueue_fd_, EPOLL_CTL_ADD, handle, &epev);
    }
  case unix_reactor::clr_mask: 
    return ::kqueue_ctl(this->kqueue_fd_, EPOLL_CTL_DEL, handle, &epev);	
  default: return -1;
  }
  return 0;
}
} // namespace ndk

