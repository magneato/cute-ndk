// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-05-14 14:57
 * Brief    : For testing multi thread file I/O.
 */
//========================================================================

#include <deque>
#include <string>
#include <fstream>
#include <cassert>
#include <iomanip>
#include <signal.h>
#include <iostream>
#include <algorithm>
#include <ndk/task.h>
#include <ndk/guard.h>
#include <ndk/types.h>
#include <ndk/logger.h>
#include <ndk/condition.h>

static ndk::logger *main_log = ndk::log_manager::instance()->get_logger("root.aio_thread");

enum
{
  AIO_READ,

  AIO_WRITE,

  AIO_CANCEL,

  // error code
  AIO_WOULDBLOCK,

  AIO_CANCELED,

  AIO_NOTCANCELED
};

#define MAX_AIO_QUEUE_LIST_SIZE  4096

class aio_opt_t;
class asynch_handler
{
public:
  virtual void handle_aio_read(const aio_opt_t *) { }

  virtual void handle_aio_write(const aio_opt_t *) { }
};
class aio_opt_t
{
  friend class asynch_file_io;
public:
  aio_opt_t()
    : handle_(NDK_INVALID_HANDLE),
    opcode_(-1),
    errcode_(0),
    priority_(0),
    i_nbytes_(0),
    o_nbytes_(0),
    offset_(0),
    aio_handler_(0),
    next_(0)
  { }

  inline ndk::ndk_handle handle() const
  { return this->handle_; }

  inline int priority() const
  { return this->priority_; }

  inline size_t bytes_completed() const
  { return this->o_nbytes_; }

  inline uint64_t offset() const
  { return this->offset_; }

  inline int errcode() const
  { return this->errcode_; }
protected:
  void reset()
  {
    this->handle_   = NDK_INVALID_HANDLE;
    this->opcode_   = -1;
    this->errcode_  =  0;
    this->priority_ = 0;
    this->i_nbytes_ = 0;
    this->o_nbytes_ = 0;
    this->offset_   = 0;
    this->aio_handler_ = 0;
    this->next_ = 0;
  }
protected:
  ndk::ndk_handle handle_;
  int opcode_;
  int errcode_;
  int priority_;
  size_t i_nbytes_;
  size_t o_nbytes_;
  uint64_t offset_;
  char *buffer_;
  asynch_handler *aio_handler_;

  aio_opt_t *next_;
};
class asynch_file_io: public ndk::task_base
{
public:
  asynch_file_io()
    : queue_list_size_(0),
    queue_list_(0),
    queue_list_mtx_(),
    not_empty_cond_(queue_list_mtx_),
    running_list_(0),
    free_list_(0)
  { }

  int open(int thr_num = 2)
  {
    return this->activate(ndk::thread::thr_join, thr_num);
  }
  virtual int svc()
  {
    ndk::message_block *aio_list = 0;
    while (1)
    {
      aio_opt_t *running_list = 0;
      {
        ndk::guard<ndk::thread_mutex> g(this->queue_list_mtx_);
        while (this->queue_list_ == 0)
        {
          this->not_empty_cond_.wait(0);
        }
        aio_opt_t *tail = this->queue_list_;
        aio_opt_t *prev = tail;
        running_list = this->queue_list_;
        for (int i = 0; i < 5 && tail->next_ != 0; ++i)
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
  void handle_aio_requests(aio_opt_t *running_list)
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
      if (result > 0)
      {
        aioopt->errcode_  = 0;
        aioopt->o_nbytes_ = result;
      }else
      {
        aioopt->errcode_  = errno;
      }
      this->dequeue_from_running_list(aioopt, callback);
    }
    this->free_aio_opt(running_list);
  }
  int start_aio(ndk::ndk_handle handle,
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
    
    aioopt->aio_handler_ = handler;
    aioopt->handle_   = handle;
    aioopt->i_nbytes_ = nbytes;
    aioopt->buffer_   = buff;
    aioopt->offset_   = offset;
    aioopt->opcode_   = optcode;
    aioopt->priority_ = priority;

    return this->enqueue_aio_request(aioopt);
  }
  /**
   * cancel an asynchronous I/O request
   * Return AIO_CANCELED the requested operation(s) were canceled.
   *        AIO_NOTCANCELED at least one of the requested operation(s) 
   *        cannot be canceled because it is in progress.
   */
  int cancel_aio(ndk::ndk_handle handle)
  {
    assert(handle != NDK_INVALID_HANDLE);
    int result = AIO_CANCELED;
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
      }
    }

    // 
    ndk::guard<ndk::thread_mutex> g(this->running_list_mtx_);
    if (this->running_list_ == 0) return result;

    aio_opt_t *itor = this->running_list_;
    while (itor != 0)
    {
      if (itor->handle_ == handle)
      {
        result = AIO_NOTCANCELED;
        break;
      }
    }

    return result;
  }
protected:
  int enqueue_aio_request(aio_opt_t *aioopt)
  {
    ndk::guard<ndk::thread_mutex> g(this->queue_list_mtx_);
    // Simply enqueue it after the running one according to the priority.
    aio_opt_t *itor = this->queue_list_;
    while (itor->next_ && aioopt->priority_ <= itor->next_->priority_)
      itor = itor->next_;

    aioopt->next_ = itor->next_;
    itor->next_ = aioopt;

    this->not_empty_cond_.signal();

    return 0;
  }
  void enqueue_to_running_list(aio_opt_t *running_list)
  {
    ndk::guard<ndk::thread_mutex> g(this->running_list_mtx_);
    aio_opt_t *itor = running_list;
    if (this->running_list_ == 0)
      this->running_list_ = running_list;
    else
    {
      this->running_list_->next_;
      aio_opt_t *tail = this->running_list_;
      while (tail->next_ != 0) tail = tail->next_;
      tail->next_ = running_list; 
    }
  }
  void dequeue_from_running_list(aio_opt_t *aioopt, 
                                 void (asynch_handler::*callback)(const aio_opt_t *))
  {
    ndk::guard<ndk::thread_mutex> g(this->running_list_mtx_);
    (aioopt->aio_handler_->*callback)(aioopt);
    assert(this->running_list_ != 0);

    aio_opt_t *itor = this->running_list_;
    aio_opt_t *prev = itor;
    while (itor != 0)
    {
      if (itor == aioopt)
      {
        if (itor == prev)
        {
          itor = itor->next_;
          this->running_list_ = itor;
          prev = itor;
        }else
        {
          prev->next_ = itor->next_; 
          itor = prev->next_;
        }
      }else
      {
        prev = itor;
        itor = itor->next_;
      }
    }
  }
  aio_opt_t *alloc_aio_opt()
  {
    ndk::guard<ndk::thread_mutex> g(this->free_list_mtx_);
    if (this->queue_list_size_ > MAX_AIO_QUEUE_LIST_SIZE)
      return 0;
    if (this->free_list_ == 0)
      this->free_list_ = new aio_opt_t();

    aio_opt_t *aioopt = this->free_list_;
    this->free_list_ = this->free_list_->next_;
    aioopt->reset();
    ++this->queue_list_size_;
    return aioopt;
  }
  void free_aio_opt(aio_opt_t *p)
  {
    ndk::guard<ndk::thread_mutex> g(this->free_list_mtx_);
    aio_opt_t *last = p;
    for (; last->next_ != 0; last = last->next_)
    last->next_ = this->free_list_;
    this->free_list_ = p;
    --this->queue_list_size_;
  }
protected:
  size_t queue_list_size_;

  aio_opt_t *requests_; // array
  aio_opt_t *queue_list_;  // single link
  ndk::thread_mutex queue_list_mtx_;
  ndk::condition<ndk::thread_mutex> not_empty_cond_;

  aio_opt_t *completed_list_;

  aio_opt_t *running_list_;
  ndk::thread_mutex running_list_mtx_;

  aio_opt_t *free_list_;
  ndk::thread_mutex free_list_mtx_;
};
