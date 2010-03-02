// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-21 16:43
 */
//========================================================================

#ifndef NDK_TASK_H_
#define NDK_TASK_H_

#include "ndk/task_base.h"
#include "message_queue.h"

namespace ndk
{
  /**
   * @class task
   * 
   * @brief
   */
  class task : public task_base
  {
  public:
    /**
     * Initialize a task, supplying a thread manager and a message
     * queue.  If the user doesn't supply a message_queue pointer
     * then we'll allocate one dynamically.  Otherwise, we'll use the
     * one passed as a parameter.
     */
    task(thread_manager *thr_mgr = 0,
         message_queue *mq = 0)
      : task_base(thr_mgr),
      delete_msg_queue_(0),
      msg_queue_(0)
    { 
      if (mq == 0)
      {
        mq = new message_queue;
        if (mq) delete_msg_queue_ = 1;
      }
      if (mq) msg_queue_ = mq;
    }

    // Destructor
    virtual ~task()
    {
      if (this->delete_msg_queue_)
        delete this->msg_queue_;
      this->delete_msg_queue_ = 0;
    }

    // Gets the message queue associated with this task.
    message_queue *msg_queue(void)
    { return this->msg_queue_; }

    // Sets the message queue associated with this task.
    void msg_queue(message_queue *msg_queue)
    { 
      if (this->delete_msg_queue_)
      {
        delete this->msg_queue_;
        this->delete_msg_queue_ = 0;
      }
      this->msg_queue_ = msg_queue;
    }

    // Insert message into the message queue tail.
    int putq(message_block *mb, const time_value* timeout = 0)
    { return this->msg_queue_->enqueue_tail(mb, timeout); }

    // Extract the first message from the queue. 
    int getq(message_block *&mb, const time_value* timeout = 0)
    { return this->msg_queue_->dequeue_head(mb, timeout); }

    // Extract n message from the queue head;
    int getq_n(message_block *&mb, int number = -1, 
               const time_value* timeout = 0)
    { return this->msg_queue_->dequeue_head_n(mb, number, timeout); }

    // Return a message to the queue.
    int ungetq(message_block *mb, const time_value* timeout = 0)
    { return this->msg_queue_->enqueue_head(mb, timeout); }

  protected:
    // 1 if should delete message_queue, 0 otherwise.
    int delete_msg_queue_;

    // Queue of messages on the Task
    message_queue *msg_queue_;
  };
} // namespace ndk

#endif // NDK_TASK_H_

