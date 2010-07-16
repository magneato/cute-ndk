#include "ndk/asynch_file_io.h"

#include <limits.h>
#include <cassert>
#include <unistd.h>

namespace ndk
{
void aio_opt_t::reset()
{
  this->handle_       = NDK_INVALID_HANDLE;
  this->opcode_       = -1;
  this->errcode_      = 0;
  this->fd_prio_      = 0;
  this->io_prio_      = 0;
  this->id_           = -1;
  this->i_nbytes_     = 0;
  this->o_nbytes_     = 0;
  this->offset_       = 0;
  this->buffer_       = 0;
  this->aio_handler_  = 0;
  this->prev_         = 0;
  this->next_         = 0;
  this->ptr_          = 0;
  this->header_       = 0;
}
int asynch_file_io::open(size_t thr_num)
{
  int size = ndk::max_handles();
  if (size <= 0) return -1;

  this->fd_pool_size_ = size;

  this->fd_pool_ = new aio_opt_t*[this->fd_pool_size_];
  for (int i = 0; i < this->fd_pool_size_; ++i)
    this->fd_pool_[i] = 0;

  return this->activate(ndk::thread::thr_join, thr_num);
}
int asynch_file_io::start_aio(ndk::ndk_handle handle,
                              int *aio_id,
                              size_t nbytes,
                              uint64_t offset,
                              char *buff,
                              asynch_handler *handler,
                              int optcode,
                              int fd_priority,
                              int io_priority)
{
  assert(nbytes > 0);
  assert(handle != NDK_INVALID_HANDLE);
  assert(handler != 0);
  assert(optcode == AIO_READ || optcode == AIO_WRITE);
  assert(handle >= 0 && handle < this->fd_pool_size_);
  assert(aio_id != 0);

  aio_opt_t *aioopt = this->alloc_aio_opt();
  if (aioopt == NULL) return -1;

  // guard
  ndk::guard<ndk::thread_mutex> g(this->queue_list_mtx_);

  int id = ++this->id_itor_;
  if (this->id_itor_ == INT_MAX)
    this->id_itor_ = 0;

  // assign data struct.
  aioopt->aio_handler_  = handler;
  aioopt->handle_       = handle;
  aioopt->i_nbytes_     = nbytes;
  aioopt->buffer_       = buff;
  aioopt->offset_       = offset;
  aioopt->opcode_       = optcode;
  aioopt->fd_prio_      = fd_priority;
  aioopt->io_prio_      = io_priority;
  aioopt->id_           = id;
  //
  *aio_id = id;

  aio_opt_t *top_node = 0;
  if (this->fd_pool_[handle] == 0)
  {
    // slot is empty.
    this->fd_pool_[handle] = aioopt;
    top_node = aioopt;

    if (this->queue_list_ == 0)
    {
      this->queue_list_ = top_node;
    }else
    {
      // keep order.
      if (top_node->fd_prio_ > this->queue_list_->fd_prio_)
      {
        top_node->next_ = this->queue_list_;
        this->queue_list_->prev_ = top_node;

        this->queue_list_ = top_node;
        this->queue_list_->prev_ = 0;
      }else
      {
        aio_opt_t *itor = this->queue_list_;
        while (itor->next_ 
               && (top_node->fd_prio_ <= itor->next_->fd_prio_))
          itor = itor->next_;

        top_node->next_ = itor->next_;
        top_node->prev_ = itor;
        if (itor->next_)
          itor->next_->prev_ = top_node;
        itor->next_ = top_node;
      }
    } // end of `if (this->queue_list_ == 0)'
  }else  // end of `if (this->fd_pool_[handle] == 0)'
    // link to sub queue
  {
    assert(this->queue_list_ != 0);
    top_node = this->fd_pool_[handle];
    if (aioopt->io_prio_ > top_node->io_prio_)
    {
      // replace old top node.
      aioopt->fd_prio_ = top_node->fd_prio_; // in order to keep fd sequence.
      this->fd_pool_[handle] = aioopt;
      aioopt->header_ = top_node;

      // reset header.
      if (this->queue_list_ == top_node)
        this->queue_list_ = aioopt;

      // insert new node.
      aioopt->next_ = top_node->next_;
      if (top_node->next_)
        top_node->next_->prev_ = aioopt;
      aioopt->prev_ = top_node->prev_;
      if (top_node->prev_)
        top_node->prev_->next_ = aioopt;

      // handle sub queue.
      top_node->next_ = top_node->header_;
      top_node->header_ = 0;
    }else  // append to sub queue
    {
      if (top_node->header_ == 0)
      {
        top_node->header_ = aioopt;
      }else
      {
        if (aioopt->io_prio_ > top_node->header_->io_prio_)
        {
          aioopt->next_ = top_node->header_;
          top_node->header_ = aioopt;
        }else
        {
          aio_opt_t *itor = top_node->header_;
#if 0
          while (itor->next_ 
                 && aioopt->io_prio_ <= itor->next_->io_prio_)
            itor = itor->next_;
#else
          while (itor->next_)
          {
            if (aioopt->io_prio_ > itor->next_->io_prio_)
              break;
            if (aioopt->io_prio_ == itor->next_->io_prio_
                && aioopt->offset_ < itor->next_->offset_)
              break;
            itor = itor->next_;
          }
#endif
          aioopt->next_ = itor->next_;
          itor->next_ = aioopt;
        }
      }
    }
  }

  this->not_empty_cond_.signal(); //

  return id;
}
int asynch_file_io::cancel_aio(ndk::ndk_handle handle, int id)
{
  assert(handle != NDK_INVALID_HANDLE 
         && (handle > 0 && handle < this->fd_pool_size_)
         && id > 0);

  ndk::guard<ndk::thread_mutex> g(this->queue_list_mtx_);
  if (this->fd_pool_[handle] == 0) 
  {
    return this->find_in_running_list(id) == 0
      ? AIO_CANCELED : AIO_NOTCANCELED;
  }

  //
  aio_opt_t *top_node = this->fd_pool_[handle];

  if (top_node->id_ == id)
  {
    if (top_node->header_ == 0)
    {
      if (top_node->prev_)
        top_node->prev_->next_ = top_node->next_;
      if (top_node->next_)
        top_node->next_->prev_ = top_node->prev_;

      if (this->queue_list_ == top_node)
        this->queue_list_ = this->queue_list_->next_;

      this->fd_pool_[handle] = 0;
    }else // has sub queue
    {
      top_node->header_->fd_prio_ = top_node->fd_prio_;

      this->fd_pool_[handle] = top_node->header_;
      this->fd_pool_[handle]->header_ = top_node->header_->next_;

      // insert new node.
      if (top_node->prev_)
        top_node->prev_->next_ = this->fd_pool_[handle];
      this->fd_pool_[handle]->prev_ = top_node->prev_;
      if (top_node->next_)
        top_node->next_->prev_ = this->fd_pool_[handle];
      this->fd_pool_[handle]->next_ = top_node->next_;

      //
      if (this->queue_list_ == top_node)
        this->queue_list_ = top_node->header_;
    }

    // release.
    this->free_aio_opt(top_node);
  }else if (top_node->header_ != 0)
  {
    if (top_node->header_->id_ == id)
    {
      aio_opt_t *item = top_node->header_;
      top_node->header_ = item->next_;
      this->free_aio_opt(item);
    }else
    {
      aio_opt_t *itor = top_node->header_;
      for (; itor->next_ != 0; itor = itor->next_)
      {
        if (itor->next_->id_ == id)
        {
          aio_opt_t *item = itor->next_;
          itor->next_ = item->next_;
          this->free_aio_opt(item);
          break;
        }
      }
    }
  }

  return this->find_in_running_list(id) == 0 ? 
    AIO_CANCELED : AIO_NOTCANCELED;
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

      running_list = this->pop_some_request(running_list, 5);

      this->enqueue_to_running_list(running_list);
    }
    this->handle_aio_requests(running_list);
  }
  return 0;
}
aio_opt_t *asynch_file_io::pop_some_request(aio_opt_t *pop_list, int num)
{
  /**
   * Pop out from the front of the queue some requests.
   * If the slot of <fd> has a sub-queue, then pop all of this sub-queue.
   */
  aio_opt_t *pop_list_itor = 0;
  aio_opt_t *top_node = this->queue_list_;

  for (int i = 0; i < num && top_node != 0; ++i)
  {
    this->fd_pool_[top_node->handle_] = 0;
    // link top node.
    if (pop_list == 0)
    {
      pop_list = top_node;
      pop_list_itor = pop_list;
    }else
    {
      pop_list_itor->next_ = top_node;
      pop_list_itor = pop_list_itor->next_;
    }

    // link sub queue.
    aio_opt_t *node = top_node->header_;

    /**
     * Important !!!.
     * because pop_list_itor point to top node and 
     * pop_list_itor->next_ will be repoint, so we must
     * store next top node.
     */
    top_node = top_node->next_; 

    for (; node != 0; ++i, node = node->next_)
    {
      pop_list_itor->next_ = node;
      pop_list_itor = pop_list_itor->next_;
    }
  }
  this->queue_list_ = top_node;
  if (this->queue_list_ != 0)
    this->queue_list_->prev_ = 0;
  if (pop_list_itor)
    pop_list_itor->next_ = 0;

  return pop_list;
}
void asynch_file_io::handle_aio_requests(aio_opt_t *running_list)
{
  for (aio_opt_t *aioopt = running_list; aioopt != 0; aioopt = aioopt->next_)
  {
    int result = 0;
    void (asynch_handler::*callback)(const aio_opt_t *) = 0;
    switch(aioopt->opcode_)
    {
    case AIO_READ:
      do
      {
        result = ::pread(aioopt->handle_,
                         (void*)aioopt->buffer_,
                         aioopt->i_nbytes_,
                         aioopt->offset_);
      }while (result == -1 && errno == EINTR);
      callback = &asynch_handler::handle_aio_read;
      break;
    case AIO_WRITE:
      do
      {
        result = ::pwrite(aioopt->handle_,
                          (const void*)aioopt->buffer_,
                          aioopt->i_nbytes_,
                          aioopt->offset_);
      }while (result == -1 && errno == EINTR);
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
  this->free_aio_opt_n(running_list);
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
    for (; itor->next_ != 0; itor = itor->next_)
    {
      if (itor->next_->ptr_ == aioopt)
      {
        if (itor->next_ == this->running_list_tail_)
          this->running_list_tail_ = itor;
        free_ptr = itor->next_;
        itor->next_ = itor->next_->next_;
        break;
      }
    }
  }
  assert(free_ptr != 0);
  free_ptr->next_ = 0;
  this->free_aio_opt_i(free_ptr, this->free_running_list_);

  if (this->running_list_ == 0 
      || this->running_list_->next_ == 0)
    this->running_list_tail_ = this->running_list_;
}
int asynch_file_io::find_in_running_list(int id)
{
  ndk::guard<ndk::thread_mutex> g(this->running_list_mtx_);
  if (this->running_list_ == 0) 
    return 0;

  aio_opt_t *itor = this->running_list_;
  for (; itor != 0; itor = itor->next_)
    if (itor->ptr_->id_ == id) return -1;
  return 0;
}
void asynch_file_io::free_aio_opt_n(aio_opt_t *p)
{
  ndk::guard<ndk::thread_mutex> g(this->free_list_mtx_);
  aio_opt_t *tail = p;
  --this->queue_list_size_;
  while (tail->next_)
  {
    tail = tail->next_;
    --this->queue_list_size_;
  }
  tail->next_ = this->free_list_;
  this->free_list_ = p;
}
#ifdef NDK_DUMP
void asynch_file_io::dump()
{
}
#endif
} // namespace ndk

