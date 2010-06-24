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
    aio_opt_t() { reset();ndk::guard<ndk::thread_mutex> g(aio_opt_t::count_mtx); aio_opt_t::count++;}
    ~aio_opt_t() { ndk::guard<ndk::thread_mutex> g(aio_opt_t::count_mtx); aio_opt_t::count--; }

    inline ndk::ndk_handle handle() const
    { return this->handle_; }

    inline int fd_prio() const
    { return this->fd_prio_; }

    inline int io_prio() const
    { return this->io_prio_; }

    inline size_t bytes_completed() const
    { return this->o_nbytes_; }

    inline uint64_t offset() const
    { return this->offset_; }

    inline int errcode() const
    { return this->errcode_; }

    inline char *buffer() const
    { return this->buffer_; }

    inline int id() const
    { return this->id_; }

  protected:
    void reset();

  protected:
    static int count;
    static ndk::thread_mutex count_mtx;
    ndk::ndk_handle handle_;
    int opcode_;
    int errcode_;
    int fd_prio_;
    int io_prio_;
    int id_;
    size_t i_nbytes_;
    size_t o_nbytes_;
    uint64_t offset_;
    char *buffer_;
    asynch_handler *aio_handler_;

    aio_opt_t *prev_;
    aio_opt_t *next_;
    aio_opt_t *ptr_;
    aio_opt_t *header_; // single link list.
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
      : id_itor_(0),
      fd_pool_size_(0),
      max_request_queue_size_(max_queue_size),
      queue_list_size_(0),
      fd_pool_(0),
      queue_list_(0),
      queue_list_mtx_(),
      not_empty_cond_(queue_list_mtx_),
      running_list_(0),
      running_list_tail_(0),
      free_running_list_(0),
      free_list_(0)
    { }

    int open(size_t thr_num);

    /**
     * post a aio request.
     * Return -1: is error that current requests are too many.
     *
     *         0: is ok.
     */
    int start_aio(ndk::ndk_handle handle,
                  int *aio_id,
                  size_t nbytes,
                  uint64_t offset,
                  char *buff,
                  asynch_handler *handler,
                  int optcode,
                  int fd_priority,
                  int io_priority);

    /**
     * cancel an asynchronous I/O request
     * Return AIO_CANCELED the requested operation(s) were canceled.
     *        AIO_NOTCANCELED at least one of the requested operation(s) 
     *        cannot be canceled because it is in progress.
     */
    int cancel_aio(ndk::ndk_handle handle, int id);

    // for debug
#ifdef NDK_DUMP
    void dump();
#endif
  protected:
    virtual int svc();

    aio_opt_t *pop_some_request(aio_opt_t *pop_list, int num);

    void handle_aio_requests(aio_opt_t *running_list);

    int enqueue_aio_request(aio_opt_t *aioopt);

    void enqueue_to_running_list(aio_opt_t *running_list);

    void dequeue_from_running_list(aio_opt_t *aioopt);

    int find_in_running_list(ndk::ndk_handle handle);

    aio_opt_t *alloc_aio_opt();

    aio_opt_t *alloc_aio_opt_i(aio_opt_t *&aio_list)
    {
      return new aio_opt_t();
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
      this->free_aio_opt_i(p, this->free_list_);
      --this->queue_list_size_;
    }
    inline void free_aio_opt_n(aio_opt_t *p);

    //
    void free_aio_opt_i(aio_opt_t *p, aio_opt_t *&aio_list);
  protected:
    int id_itor_;
    int fd_pool_size_;
    size_t max_request_queue_size_;
    size_t queue_list_size_;

    aio_opt_t **fd_pool_;  // arrary 
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

