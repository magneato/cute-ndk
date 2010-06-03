#include "ndk/asynch_file_io.h"

namespace ndk
{
int asynch_file_io::start_aio(ndk::ndk_handle handle,
                              size_t nbytes,
                              uint64_t offset,
                              char *buff,
                              asynch_handler *handler,
                              int optcode,
                              int priority)
{
  assert(nbytes > 0);
  assert(handle != NDK_INVALID_HANDLE);
  assert(handler != 0);
  assert(optcode == AIO_READ || optcode == AIO_WRITE);

  aio_opt_t *aioopt = this->alloc_aio_opt();
  if (aioopt == 0) return AIO_WOULDBLOCK;

  aioopt->aio_handler_  = handler;
  aioopt->handle_       = handle;
  aioopt->i_nbytes_     = nbytes;
  aioopt->buffer_       = buff;
  aioopt->offset_       = offset;
  aioopt->opcode_       = optcode;
  aioopt->priority_     = priority;

  return this->enqueue_aio_request(aioopt);
}
int asynch_file_io::cancel_aio(ndk::ndk_handle handle)
{
  assert(handle != NDK_INVALID_HANDLE);
  {
    ndk::guard<ndk::thread_mutex> g(this->queue_list_mtx_);
    if (this->queue_list_ != 0) 
    {
      aio_opt_t *itor = this->queue_list_;
      aio_opt_t *prev = itor;
      while (itor != 0)
      {
        if (itor->handle_ == handle)
        {
          aio_opt_t *p = itor;
          if (itor == prev)
          {
            itor = itor->next_;
            this->queue_list_ = itor;
            prev = itor;
          }else
          {
            prev->next_ = itor->next_; 
            itor = prev->next_;
          }
          this->free_aio_opt(p);
        }else
        {
          prev = itor;
          itor = itor->next_;
        }
      }
    } // end of `if (this->queue_list_ != 0)'
  }
  return this->find_in_running_list(handle) == 0 ? AIO_CANCELED : AIO_NOTCANCELED;
}
int asynch_file_io::svc()
{
  while (1)
  {
    aio_opt_t *running_list = 0;
    {
      ndk::guard<ndk::thread_mutex> g(this->queue_list_mtx_);
      while (this->queue_list_ == 0)
        this->not_empty_cond_.wait(0);

      aio_opt_t *tail = this->queue_list_->next_;
      aio_opt_t *prev = this->queue_list_;
      running_list = this->queue_list_;
      for (int i = 1; i < 5 && tail != 0; ++i)
      {
        prev = tail;
        tail = tail->next_;
      }
      prev->next_ = 0;
      this->queue_list_ = tail;
    }
    this->handle_aio_requests(running_list);
  }
  return 0;
}
void asynch_file_io::handle_aio_requests(aio_opt_t *running_list)
{
  this->enqueue_to_running_list(running_list);

  for (aio_opt_t *aioopt = running_list; aioopt != 0; aioopt = aioopt->next_)
  {
    int result = 0;
    void (asynch_handler::*callback)(const aio_opt_t *) = 0;
    switch(aioopt->opcode_)
    {
    case AIO_READ:
      result = ::pread(aioopt->handle_,
                       (void*)aioopt->buffer_,
                       aioopt->i_nbytes_,
                       aioopt->offset_);
      callback = &asynch_handler::handle_aio_read;
      break;
    case AIO_WRITE:
      result = ::pwrite(aioopt->handle_,
                        (const void*)aioopt->buffer_,
                        aioopt->i_nbytes_,
                        aioopt->offset_);
      callback = &asynch_handler::handle_aio_write;
      break;
    default:
      assert(0);
    }
    if (result >= 0)
    {
      aioopt->errcode_  = 0;
      aioopt->o_nbytes_ = result;
    }else
    {
      aioopt->errcode_  = errno;
    }
    (aioopt->aio_handler_->*callback)(aioopt);
    this->dequeue_from_running_list(aioopt);
  }
  this->free_aio_opt(running_list);
}
int asynch_file_io::enqueue_aio_request(aio_opt_t *aioopt)
{
  ndk::guard<ndk::thread_mutex> g(this->queue_list_mtx_);
  if (this->queue_list_ == 0)
  {
    this->queue_list_ = aioopt;
  }else
  {
    // Simply enqueue it after the running one according to the priority.
    aio_opt_t *itor = this->queue_list_;
    while (itor->next_ && aioopt->priority_ <= itor->next_->priority_)
    {
      itor = itor->next_;
    }

    aioopt->next_ = itor->next_;
    itor->next_ = aioopt;
  }

  this->not_empty_cond_.signal();

  return 0;
}
void asynch_file_io::enqueue_to_running_list(aio_opt_t *running_list)
{
  assert(running_list != 0);
  ndk::guard<ndk::thread_mutex> g(this->running_list_mtx_);

  aio_opt_t *aioopt = 0;
  if (this->running_list_ == 0)
  {
    aioopt = this->alloc_aio_opt_i(this->free_running_list_);
    aioopt->ptr_ = running_list;
    this->running_list_tail_ = aioopt;
    this->running_list_ = aioopt;
    running_list = running_list->next_;
  }

  aio_opt_t *itor = running_list;
  for (; itor != 0; itor = itor->next_)
  {
    aioopt = this->alloc_aio_opt_i(this->free_running_list_);
    aioopt->ptr_ = itor;
    this->running_list_tail_->next_ = aioopt;
    this->running_list_tail_ = aioopt;
  }
}
void asynch_file_io::dequeue_from_running_list(aio_opt_t *aioopt)
{
  ndk::guard<ndk::thread_mutex> g(this->running_list_mtx_);
  assert(this->running_list_ != 0);

  aio_opt_t *itor = this->running_list_;
  aio_opt_t *free_ptr = 0;
  if (itor->ptr_ == aioopt)
  {
    free_ptr = itor;
    this->running_list_ = itor->next_;
  }else
  {
    while (itor->next_ != 0)
    {
      if (itor->next_->ptr_ == aioopt)
      {
        if (itor->next_ == this->running_list_tail_)
          this->running_list_tail_ = itor;
        free_ptr = itor->next_;
        itor->next_ = itor->next_->next_;
        break;
      }
      itor = itor->next_;
    }
  }
  assert(free_ptr != 0);
  free_ptr->next_ = 0;
  this->free_aio_opt_i(free_ptr, this->free_running_list_);

  if (this->running_list_ == 0 
      || this->running_list_->next_ == 0)
    this->running_list_tail_ = this->running_list_;
}
int asynch_file_io::find_in_running_list(ndk::ndk_handle handle)
{
  ndk::guard<ndk::thread_mutex> g(this->running_list_mtx_);
  if (this->running_list_ == 0) return 0;

  aio_opt_t *itor = this->running_list_;
  while (itor != 0)
  {
    if (itor->ptr_->handle_ == handle)
      return -1;
  }
  return 0;
}
aio_opt_t *asynch_file_io::alloc_aio_opt()
{
  ndk::guard<ndk::thread_mutex> g(this->free_list_mtx_);
  if (this->queue_list_size_ > this->max_request_queue_size_)
    return 0;
  aio_opt_t *aioopt = this->alloc_aio_opt_i(this->free_list_);
  ++this->queue_list_size_;
  return aioopt;
}
int asynch_file_io::free_aio_opt_i(aio_opt_t *p, aio_opt_t *&aio_list)
{
  aio_opt_t *last = p;
  int c = 1;
  for (; last->next_ != 0; last = last->next_) ++c;
  last->next_ = aio_list;
  aio_list = p;
  return c;
}
} // namespace ndk

