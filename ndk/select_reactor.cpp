#include "ndk/select_reactor.h"

namespace ndk
{
select_reactor_base::select_reactor_base()
: unix_reactor(),
  initialized_(false),
  handle_list_rd_(0),
  handle_list_wr_(0),
  handle_list_ex_(0)
{
  rd_set_ = new fd_set;
  wr_set_ = new fd_set;
  ex_set_ = new fd_set;
}
int select_reactor_base::open_i(timer_queue *tq)
{
  STRACE("");
  if (this->initialized_) return -1;

  int size = FD_SETSIZE;

  this->free_node_ = new select_handle();
  select_handle *iter = this->free_node_;
  for (int i = 1; i < size * 3; ++i, iter = iter->next)
    iter->next = new select_handle();
  if (unix_reactor::open_ii(size, tq) != 0)
  {
    this->close_i(); return -1;
  }
  this->initialized_ = true;
  return 0;
}
int select_reactor_base::close_i()
{
  STRACE("");
  this->initialized_   = false;
  if (this->rd_set_) delete this->rd_set_; this->rd_set_ = 0;
  if (this->wr_set_) delete this->wr_set_; this->wr_set_ = 0;
  if (this->ex_set_) delete this->ex_set_; this->ex_set_ = 0;

  select_handle *iter = this->free_node_;
  while (iter != 0)
  {
    select_handle *p = iter;
    iter = iter->next;
    delete p;
  }
  this->free_node_ = 0;

  // rd.
  iter = this->handle_list_rd_;
  while (iter != 0)
  {
    select_handle *p = iter;
    iter = iter->next;
    delete p;
  }
  this->handle_list_rd_ = 0;

  // wr.
  iter = this->handle_list_wr_;
  while (iter != 0)
  {
    select_handle *p = iter;
    iter = iter->next;
    delete p;
  }
  this->handle_list_wr_ = 0;

  // ex.
  iter = this->handle_list_ex_;
  while (iter != 0)
  {
    select_handle *p = iter;
    iter = iter->next;
    delete p;
  }
  this->handle_list_ex_ = 0;

  return unix_reactor::close_ii();
}
int select_reactor_base::poll_events(const time_value *timeout)
{
  STRACE("");
  fd_set *rd_set = 0, *wr_set = 0, *ex_set = 0;

  int max_handle = this->reset_fd_set(rd_set, wr_set, ex_set);

  struct timeval tv;
  struct timeval *ptv = 0;
  if (timeout != 0)
  {
    tv = (*timeout);
    ptv = &tv;
  }
  int result = ::select(max_handle, rd_set, wr_set, ex_set, ptv);
  return result;
}
int select_reactor_base::reset_fd_set(fd_set *&rd_set, 
                                      fd_set *&wr_set,
                                      fd_set *&ex_set)
{
  STRACE("");
  FD_ZERO(this->rd_set_);
  FD_ZERO(this->wr_set_);
  FD_ZERO(this->ex_set_);
  int result = -1;
  if (this->handle_list_rd_ == 0) rd_set = 0;
  else
  {
    rd_set = this->rd_set_;
    select_handle *itor = this->handle_list_rd_;
    while (itor != 0)
    {
      if (itor->handle != NDK_INVALID_HANDLE)
      {
        FD_SET(itor->handle, rd_set);
        if (itor->handle > result) result = itor->handle;
      }
      itor = itor->next;
    }
  }
  if (this->handle_list_wr_ == 0) wr_set = 0;
  else
  {
    wr_set = this->wr_set_;
    select_handle *itor = this->handle_list_wr_;
    while (itor != 0)
    {
      if (itor->handle != NDK_INVALID_HANDLE)
      {
        FD_SET(itor->handle, wr_set);
        if (itor->handle > result) result = itor->handle;
      }
      itor = itor->next;
    }
  }
  if (this->handle_list_ex_ == 0) ex_set = 0;
  else
  {
    ex_set = this->ex_set_;
    select_handle *itor = this->handle_list_ex_;
    while (itor != 0)
    {
      if (itor->handle != NDK_INVALID_HANDLE)
      {
        FD_SET(itor->handle, ex_set);
        if (itor->handle > result) result = itor->handle;
      }
      itor = itor->next;
    }
  }
  return result + 1;
}
int select_reactor_base::dispatch_io_events()
{
  STRACE("");
  int result = 0;
  for (int i = 0; i < this->size_; ++i)
  {
    if (FD_ISSET(i, this->rd_set_))
    {
      event_handler *eh = this->handler_rep_.find(i);
      if (eh)
      {
        if (this->upcall(eh,
                         &event_handler::handle_input,
                         i) < 0)
          this->remove_handler_i(i, event_handler::read_mask);
      }
    }
    if (FD_ISSET(i, this->wr_set_))
    {
      event_handler *eh = this->handler_rep_.find(i);
      if (eh)
      {
        if (this->upcall(eh,
                         &event_handler::handle_output,
                         i) < 0)
          this->remove_handler_i(i, event_handler::write_mask);
      }
    }
    if (FD_ISSET(i, this->ex_set_))
    {
      event_handler *eh = this->handler_rep_.find(i);
      if (eh)
      {
        if (this->upcall(eh,
                         &event_handler::handle_exception,
                         i) < 0)
          this->remove_handler_i(i, event_handler::except_mask);
      }
    }
    ++result;
  }
  return result;
}
int select_reactor_base::handle_opt_i(ndk_handle handle,
                                     reactor_mask mask,
                                     int opt)
{
  STRACE("");

  if (mask == event_handler::null_mask)
    opt = unix_reactor::clr_mask;
  else if (opt == unix_reactor::clr_mask)
    opt = unix_reactor::set_mask;

  switch(opt)
  {
  case unix_reactor::add_mask: 
    return this->reactor_mask_to_select_event(handle, mask);
  case unix_reactor::set_mask: 
    return this->reactor_mask_to_select_event(handle, mask);
  case unix_reactor::clr_mask: 
    return this->reactor_mask_to_select_event(handle, mask);
  default: return -1;
  }
  return 0;
}
int select_reactor_base::reactor_mask_to_select_event(ndk_handle handle, 
                                                      reactor_mask mask)
{
  STRACE("");
  if (mask == event_handler::null_mask)
  {
    if (this->handle_list_rd_)
      this->remove_handle(handle, this->handle_list_rd_);
    if (this->handle_list_wr_)
      this->remove_handle(handle, this->handle_list_wr_);
    if (this->handle_list_ex_)
      this->remove_handle(handle, this->handle_list_ex_);
    return 0;
  }

  bool readable = false, writeable = false, exception = false;
  if (NDK_BIT_ENABLED(mask, event_handler::read_mask))
    readable = true;

  if (NDK_BIT_ENABLED(mask, event_handler::accept_mask))
    readable = true;

  if (NDK_BIT_ENABLED(mask, event_handler::connect_mask))
  {
    readable = true; writeable = true;
  }

  if (NDK_BIT_ENABLED(mask, event_handler::write_mask))
    writeable = true;

  if (NDK_BIT_ENABLED(mask, event_handler::except_mask))
    exception = true;

  if (readable)
  {
    if (this->append_handle(handle, this->handle_list_rd_) != 0)
      return -1;
  }
  if (writeable)
  {
    if (this->append_handle(handle, this->handle_list_wr_) != 0)
      return -1;
  }
  if (exception)
  {
    if (this->append_handle(handle, this->handle_list_ex_) != 0)
      return -1;
  }
  return 0;
}
int select_reactor_base::append_handle(ndk_handle handle, select_handle *&sh)
{
  STRACE("");
  if (sh == 0)
  {
    sh = this->alloc_handle();
    if (sh == 0) return -1;
  }else
  {
    select_handle *h = this->alloc_handle();
    if (h == 0) return -1;
    h->next = sh; sh = h;
  }
  sh->handle = handle;
  return 0;
}
int select_reactor_base::remove_handle(ndk_handle handle, select_handle *&sh)
{
  STRACE("");
  if (sh->handle == handle)
  {
    select_handle *p = sh;
    sh = sh->next;
    this->release_handle(p);
  }else
  {
    select_handle *prev = sh;
    select_handle *itor = sh->next;
    while (itor != 0)
    {
      if (itor->handle == handle)
      {
        prev->next = itor->next;
        this->release_handle(itor);
        break;
      }
      prev = itor;
      itor = itor->next;
    }
  }
  return 0;
}
} // namespace ndk
