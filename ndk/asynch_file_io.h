// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-05-24 11:08
 */
//========================================================================

#ifndef NDK_ASYNCH_FILE_IO_H_
#define NDK_ASYNCH_FILE_IO_H_

#include <stdint.h>

#include "ndk/types.h"
#include "ndk/guard.h"
#include "ndk/config.h"
#include "ndk/task_base.h"
#include "ndk/condition.h"
#include "ndk/thread_mutex.h"

namespace ndk
{
  /**
   * @class enum for aio operation.
   * 
   * @brief
   */
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
  /**
   * @class asynch_handler
   * 
   * @brief
   */
  class aio_opt_t;
  class asynch_handler
  {
  public:
    virtual ~asynch_handler() { }

    virtual void handle_aio_read(const aio_opt_t *) { }

    virtual void handle_aio_write(const aio_opt_t *) { }
  protected:
    //
    asynch_handler() { }
  };
  /**
   * @class aio_opt_t
   * 
   * @brief
   */
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
      buffer_(0),
      aio_handler_(0),
      next_(0),
      ptr_(0)
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

    inline char *buffer() const
    { return this->buffer_; }
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
      this->buffer_   = 0;
      this->aio_handler_ = 0;
      this->next_ = 0;
      this->ptr_  = 0;
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
    aio_opt_t *ptr_;
  };
  /**
   * @class asynch_file_io
   * 
   * @brief
   */
  class asynch_file_io: public ndk::task_base
  {
  public:
    asynch_file_io(size_t max_queue_size)
      : max_request_queue_size_(max_queue_size),
      queue_list_size_(0),
      queue_list_(0),
      queue_list_mtx_(),
      not_empty_cond_(queue_list_mtx_),
      running_list_(0),
      running_list_tail_(0),
      free_running_list_(0),
      free_list_(0)
    { }

    inline int open(size_t thr_num)
    {
      return this->activate(ndk::thread::thr_join, thr_num);
    }

    //
    int start_aio(ndk::ndk_handle handle,
                  size_t nbytes,
                  uint64_t offset,
                  char *buff,
                  asynch_handler *handler,
                  int optcode,
                  int priority);

    /**
     * cancel an asynchronous I/O request
     * Return AIO_CANCELED the requested operation(s) were canceled.
     *        AIO_NOTCANCELED at least one of the requested operation(s) 
     *        cannot be canceled because it is in progress.
     */
    int cancel_aio(ndk::ndk_handle handle);
  protected:
    virtual int svc();

    void handle_aio_requests(aio_opt_t *running_list);

    int enqueue_aio_request(aio_opt_t *aioopt);

    void enqueue_to_running_list(aio_opt_t *running_list);

    void dequeue_from_running_list(aio_opt_t *aioopt);

    int find_in_running_list(ndk::ndk_handle handle);

    aio_opt_t *alloc_aio_opt();

    aio_opt_t *alloc_aio_opt_i(aio_opt_t *&aio_list)
    {
      if (aio_list == 0) aio_list = new aio_opt_t();
      aio_opt_t *aioopt = aio_list;
      aio_list = aio_list->next_;
      aioopt->reset();
      return aioopt;
    }

    //
    inline void free_aio_opt(aio_opt_t *p)
    {
      ndk::guard<ndk::thread_mutex> g(this->free_list_mtx_);
      int c = this->free_aio_opt_i(p, this->free_list_);
      this->queue_list_size_ -= c;
    }

    //
    int free_aio_opt_i(aio_opt_t *p, aio_opt_t *&aio_list);
  protected:
    size_t max_request_queue_size_;
    size_t queue_list_size_;

    aio_opt_t *queue_list_;  // single link
    ndk::thread_mutex queue_list_mtx_;
    ndk::condition<ndk::thread_mutex> not_empty_cond_;

    aio_opt_t *running_list_;
    aio_opt_t *running_list_tail_;
    aio_opt_t *free_running_list_;
    ndk::thread_mutex running_list_mtx_;

    aio_opt_t *free_list_;
    ndk::thread_mutex free_list_mtx_;
  };
} // namespace ndk

#endif // NDK_ASYNCH_FILE_IO_H_

