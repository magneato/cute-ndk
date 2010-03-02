// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 09:44
 */
//========================================================================

#ifndef NDK_MESSAGE_QUEUE_H_
#define NDK_MESSAGE_QUEUE_H_

#include "ndk/guard.h"
#include "ndk/condition.h"
#include "ndk/thread_mutex.h"
#include "ndk/message_block.h"

#define  DEFAULT_HIGH_WATER_MARK   (1024*2)

namespace ndk
{
  /**
   * @class message_queue
   *
   * @brief This class can be used to communicate between muilt-threads, notify 
   * mechanism use Condition.
   */
  class message_queue
  {
  public:
    /**
     * <high_water_mark> is the max number of the message_block in the 
     * message_queue in concurrent time, If <mb_count_> is greater than
     * <high_water_mark>, then enqueue_* operation will blocked or failed.
     *
     * This mark is like to TCP buffer's water mark, but we haven't lower
     * water mark and our mark value is the number of message blocks, not
     * bytes of buffer size. 
     */
    message_queue(size_t high_water_mark = DEFAULT_HIGH_WATER_MARK)
      : mb_count_(0),
      high_water_mark_(high_water_mark),
      head_(0),
      tail_(0),
      queue_mutex_(),
      not_empty_cond_(queue_mutex_),
      not_full_cond_(queue_mutex_)
    {}

    // releases all resources(message_block) store in the message queue
    virtual ~message_queue();

    /**
     * @param new_item can be a list or a single message_block,
     *	      timeout: = 0, block;  nonzero , block until timeout
     * return -1: enqueue failed
     * return >0: the number of mbs enqueue successfully
     * return  0: enqueue 0 mb 
     */
    int enqueue_tail(message_block *new_item, 
                     const time_value *timeout = 0);

    /**
     * @param new_item can be a list or a single message_block,
     *	      timeout: = 0, block;  nonzero , block until timeout
     * return -1: enqueue failed
     * return >0: the number of mbs enqueue successfully
     * return  0: enqueue 0 mb 
     */
    int enqueue_head(message_block *new_item, 
                     const time_value *timeout = 0);

    /**
     * @param first_item: reference to a mb-pointer that will be set 
     * to the address of the dequeued block
     *	      timeout: = 0, block;  nonzero , block until timeout
     * return -1: dequeue failed, timeout or error
     * return  0: dequeue successfully
     */
    int dequeue_head(message_block *&first_item,  
                     const time_value *timeout = 0); 

    /**
     * This method can dequeue the <number> of the message_block that in 
     * message_queue, it will dequeue all of the member in the message_queue
     * if <number> = -1, 
     * return the number of dequeued successfully actually until <timeout> 
     * timeout. dequeue items from message_block as best as it can if timeout
     * is nonzero. if timeout == 0, it return until dequeue the <number> 
     * of the message_block
     * Note : items is a list, you must check items->next() != 0
     */ 
    int dequeue_head_n(message_block *&items, int number = -1, 
                       const time_value *timeout = 0);

    /**
     * @param last_item: reference to a mb-pointer that will be set 
     * to the address of the dequeued block
     * @param timeout: = 0, block;  nonzero , block until timeout
     * return -1: dequeue failed, timeout or error
     * return  0: dequeue successfully
     */
    int dequeue_tail(message_block *&last_item, 
                     const time_value *timeout = 0); 

    /**
     * This method can dequeue the <number> of the message_block that in 
     * message_queue, it will dequeue all of the member in the message_queue
     * if <number> = -1, 
     * return the number of dequeued successfully actually until <timeout> 
     * timeout. dequeue items from message_block as best as it can if timeout
     * is non zero. if timeout is 0, it return until dequeue the <number> 
     * of the message_block
     * Note : items is a list, you must check items->next() != 0
     */ 
    int dequeue_tail_n(message_block *&items, int number = -1, 
                       const time_value *timeout = 0);

    // get size of message queue
    inline size_t size(void)
    {
      ndk::guard<mtx_t> g(this->queue_mutex_);
      return this->mb_count_;
    }

    // check message_queue is empty or not
    bool is_empty()
    {
      ndk::guard<mtx_t> g(this->queue_mutex_);
      return this->is_empty_i ();
    }
protected:
    //
    inline bool is_empty_i()
    { return this->tail_ == 0; }

    inline bool is_full_i()
    { return this->mb_count_ >= this->high_water_mark_; }

    int wait_not_full_cond(const time_value *timeout);

    int wait_not_empty_cond(const time_value *timeout);
    
    int enqueue_tail_i(message_block *new_item);

    int enqueue_head_i(message_block *new_item);

    int dequeue_head_i(message_block *&first_item);

    int dequeue_head_n_i(message_block *&items, int number);

    int dequeue_tail_i(message_block *&last_item);

    int dequeue_tail_n_i(message_block *&items, int number);
private:
    // Count the message block in queue
    size_t	  mb_count_;

    size_t    high_water_mark_;

    // Store header mb.
    message_block *head_;

    // Store tail mb.
    message_block *tail_;

    //
    typedef thread_mutex mtx_t;
    mtx_t     queue_mutex_;

    //
    condition<thread_mutex> not_empty_cond_;

    // For high water mark
    condition<thread_mutex> not_full_cond_;
  };
} // namespace ndk

#endif // NDK_MESSAGE_QUEUE_H_

