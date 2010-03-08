#include "ndk/epoll_reactor.h"

#ifdef __linux__

# include <cstring>
# include <cstdio>

namespace ndk
{
epoll_reactor_base::epoll_reactor_base()
: unix_reactor(),
  initialized_(false),
  epoll_fd_(NDK_INVALID_HANDLE),
  events_(0),
  start_pevents_(0),
  end_pevents_(0)
{
}
int epoll_reactor_base::open_i(timer_queue *tq)
{
  if (this->initialized_) return -1;

  int size = ndk::max_handles();
  if (size <= 0) return -1;
  this->events_ = new epoll_event[size];
  if (this->events_ == 0)
    return -1;

  // Since Linux 2.6.8, the size argument is unused.
  this->epoll_fd_ = ::epoll_create(size);
  if (this->epoll_fd_ == NDK_INVALID_HANDLE)
  {
    this->close_i(); return -1;
  }
  ::fcntl(this->epoll_fd_, F_SETFD, FD_CLOEXEC);

  if (unix_reactor::open_ii(size, tq) != 0)
  {
    this->close_i(); return -1;
  }
  this->initialized_ = true;
  return 0;
}
int epoll_reactor_base::close_i()
{
  if (this->epoll_fd_ != NDK_INVALID_HANDLE)
    ::close(this->epoll_fd_);
  this->epoll_fd_ = NDK_INVALID_HANDLE;
  if (this->events_)
    delete [] this->events_;
  this->events_ = 0;          
  this->initialized_   = false;
  return unix_reactor::close_ii();
}
int epoll_reactor_base::poll_events(const time_value *timeout)
{
  if (this->start_pevents_ != this->end_pevents_)
    return 1;
  int nfds = ::epoll_wait(this->epoll_fd_, 
                          this->events_, 
                          this->size_, 
                          timeout == 0 ?  -1 
                          : static_cast<int>(timeout->msec()));
  if (nfds > 0)
  {
    this->start_pevents_ = this->events_;
    this->end_pevents_   = this->start_pevents_ + nfds;
  }
  return nfds;
}
int epoll_reactor_base::dispatch_io_events()
{
  STRACE("");
  struct epoll_event *& pfd = this->start_pevents_;
  int result = 0;
  while (pfd < this->end_pevents_ && 
         result < MAX_REACTOR_PROCESS_FDS_ONE_TIME)
  {
    // 
    event_handler *eh = this->handler_rep_.find(pfd->data.fd);
    if (eh)
    {
      ++result;
      // Note that if there's an error(such as the handle was closed
      // without being removed from the event set) the EPOLLHUP and/or
      // EPOLLERR bits will be set in pfd->events.
      if (NDK_BIT_ENABLED(pfd->events, EPOLLIN))
      {
        NDK_CLR_BITS(pfd->events, EPOLLIN);
        if (this->upcall(eh, 
                         &event_handler::handle_input, 
                         pfd->data.fd) < 0)
          this->remove_handler_i(pfd->data.fd, 
                                 event_handler::read_mask);
      }else if (NDK_BIT_ENABLED(pfd->events, EPOLLOUT))
      {
        NDK_CLR_BITS(pfd->events, EPOLLOUT);
        if (this->upcall(eh, 
                         &event_handler::handle_output, 
                         pfd->data.fd) < 0)
          this->remove_handler_i(pfd->data.fd, 
                                 event_handler::write_mask);
      }else if (NDK_BIT_ENABLED(pfd->events, EPOLLPRI))
      {
        NDK_CLR_BITS(pfd->events, EPOLLPRI);
        if (this->upcall(eh, 
                         &event_handler::handle_exception, 
                         pfd->data.fd) < 0)
          this->remove_handler_i(pfd->data.fd, 
                                 event_handler::except_mask);
      }else if (NDK_BIT_ENABLED(pfd->events, EPOLLHUP | EPOLLERR))
      {
        this->remove_handler_i(pfd->data.fd, 
                               event_handler::all_events_mask);
        ++pfd;
      }else
      {
        NDK_LOG("dispatch_io [handle = %d] trigger unknown events 0x%x",
                pfd->data.fd, 
                pfd->events);
        --result;
        if (pfd->events != 0) ++pfd;
      } 
      // If more than one event comes in between epoll_wait(2) calls,
      // they will be combined reported.
      if (pfd->events == 0)
        ++pfd; 
    }else // if (eh)
    {
#if 0
      fprintf(stderr, 
              "dispatch_io [handle = %d] not match event handler\n", 
              pfd->data.fd);
#endif
      ++pfd; 
    }
  } // while (pfd < this->end_pevents_ ...
  return result;
}
int epoll_reactor_base::reactor_mask_to_epoll_event(reactor_mask mask)
{
  STRACE("");
  int events = 0;
  if (NDK_BIT_ENABLED(mask, event_handler::read_mask))
    NDK_SET_BITS(events, EPOLLIN);

  if (NDK_BIT_ENABLED(mask, event_handler::epoll_edge))
    NDK_SET_BITS(events, EPOLLET);

  if (NDK_BIT_ENABLED(mask, event_handler::accept_mask))
    NDK_SET_BITS(events, EPOLLIN);

  if (NDK_BIT_ENABLED(mask, event_handler::connect_mask))
    NDK_SET_BITS(events, EPOLLIN|EPOLLOUT);

  if (NDK_BIT_ENABLED(mask, event_handler::write_mask))
    NDK_SET_BITS(events, EPOLLOUT);

  if (NDK_BIT_ENABLED(mask, event_handler::except_mask))
    NDK_SET_BITS(events, EPOLLPRI);

  return events;
}
int epoll_reactor_base::handle_opt_i(ndk_handle handle,
                                     reactor_mask mask,
                                     int opt)
{
  STRACE("");
  struct epoll_event epev;
  std::memset(&epev, 0, sizeof(epev));
  epev.data.fd = handle;

  if (mask == event_handler::null_mask)
    opt = unix_reactor::clr_mask;
  else if (opt == unix_reactor::clr_mask)
    opt = unix_reactor::set_mask;

  switch(opt)
  {
  case unix_reactor::add_mask: 
    epev.events = this->reactor_mask_to_epoll_event(mask);
    return ::epoll_ctl(this->epoll_fd_, 
                       EPOLL_CTL_ADD, 
                       handle, 
                       &epev);	
  case unix_reactor::set_mask: 
    epev.events = this->reactor_mask_to_epoll_event(mask);
    if (::epoll_ctl(this->epoll_fd_, 
                    EPOLL_CTL_MOD, 
                    handle, 
                    &epev) == -1)
    {
      NDK_LOG("epoll_ctl [%s]", strerror(errno));
      // If a handle is closed, epoll removes it from the poll set
      // automatically - we may not know about it yet. If that's the
      // case, a mod operation will fail with ENOENT. Retry it as
      // an add.
      if (errno == ENOENT)
        return ::epoll_ctl(this->epoll_fd_, 
                           EPOLL_CTL_ADD, 
                           handle, 
                           &epev);
    }
    break;
  case unix_reactor::clr_mask: 
    return ::epoll_ctl(this->epoll_fd_, 
                       EPOLL_CTL_DEL, 
                       handle, 
                       &epev);	
  default: return -1;
  }
  return 0;
}
#endif // __linux__
}// namespace ndk
