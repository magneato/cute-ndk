#include "message_queue.h"

namespace ndk
{
message_queue::~message_queue()
{
  ndk::guard<mtx_t> g(this->queue_mutex_);
  if (this->head_ != 0)
  {
    message_block *mb = 0;
    for (this->tail_ = 0; this->head_ != 0; )
    {
      mb = this->head_;
      this->head_ = this->head_->next();
      mb->clean();  // 
      delete mb;
    }
  }
}
int message_queue::wait_not_full_cond(const time_value *timeout)
{
  while (this->is_full_i())
  {
    // noblock.
    if (timeout && *timeout == time_value::zero)
      return -1;

    if (this->not_full_cond_.wait(timeout) != 0)
      return -1;
  }
  return 0;
}
int message_queue::enqueue_tail(message_block *new_item, 
                                const time_value *timeout/* = 0*/)
{
  if (new_item == 0) return -1;
  // lock
  int num = 0;
  ndk::guard<mtx_t> g(this->queue_mutex_);
  if (this->wait_not_full_cond(timeout) != 0)
    return -1;
  num = this->enqueue_tail_i(new_item);
  this->not_empty_cond_.signal();
  return num;
}
int message_queue::enqueue_tail_i(message_block *new_item)
{
  // maybe new_item is a short queue
  message_block *seq_tail = new_item;
  int num = 1;

  // count the number of the sub message queue
  while (seq_tail->next() != 0)
  {
    seq_tail->next()->prev(seq_tail);
    seq_tail = seq_tail->next();
    ++num;
  }   

  // queue is empty, so build a new one
  if (this->tail_ == 0)
  {
    this->head_ = new_item;
    this->tail_ = seq_tail;
    new_item->prev(0);
  }else  // link at the end
  {
    new_item->prev(this->tail_);
    this->tail_->next(new_item);
    this->tail_ = seq_tail;
  }
  this->mb_count_ += num;
  return num;
} 
int message_queue::enqueue_head(message_block *new_item, 
                                const time_value *timeout/* = 0*/)
{
  if (new_item == 0) return -1;
  // lock
  int num = 0;
  ndk::guard<mtx_t> g(this->queue_mutex_);
  if (this->wait_not_full_cond(timeout) != 0)
    return -1;
  num = this->enqueue_head_i(new_item);
  this->not_empty_cond_.signal();
  return num;
}
int message_queue::enqueue_head_i(message_block *new_item)
{
  // maybe new_item is a short queue
  message_block *seq_tail = new_item;
  int num = 1;

  // count the number of the sub message queue
  while (seq_tail->next() != 0)
  {
    seq_tail->next()->prev(seq_tail);
    seq_tail = seq_tail->next();
    ++num;
  }   

  new_item->prev(0);   // set header's prev empty.
  seq_tail->next(this->head_);
  if (this->head_ != 0)
    this->head_->prev(seq_tail);  
  else
    this->tail_ = seq_tail;
  this->head_ = new_item;

  this->mb_count_ += num;
  return num;
} 
int message_queue::wait_not_empty_cond(const time_value *timeout)
{
  while (this->is_empty_i())
  {
    if (timeout &&(*timeout == time_value::zero))
      return -1;  // noblock
    if (this->not_empty_cond_.wait(timeout) != 0)
    {
      return -1;  // timeout
    }
  }
  return 0;
}
int message_queue::dequeue_head(message_block *&first_item, 
                                const time_value *timeout/* = 0*/)
{
  int num = 0;
  ndk::guard<mtx_t> g(this->queue_mutex_);
  if (this->wait_not_empty_cond(timeout) != 0)
    return -1;
  num = this->dequeue_head_i(first_item);
  this->not_full_cond_.signal();
  return num;
}
int message_queue::dequeue_head_i(message_block *&first_item)
{    
  if (this->head_ == 0) return -1;  // check empty !!

  first_item  = this->head_;
  this->head_ = this->head_->next();
  if (this->head_ == 0) 
    this->tail_ = 0;
  else  
    // the prev pointer of first message block must point to 0
    this->head_->prev(0);
  --this->mb_count_;
  // clean 
  first_item->prev(0);
  first_item->next(0);
  return 0;
}
int message_queue::dequeue_head_n(message_block *&items, 
                                  int number/* = -1*/, 
                                  const time_value *timeout/* = 0*/)
{
  if (number == 0) return 0;
  ndk::guard<mtx_t> g(this->queue_mutex_);
  if (this->wait_not_empty_cond(timeout) != 0)
    return -1;
  number = this->dequeue_head_n_i(items, number);
  this->not_full_cond_.signal();
  return number;
}
int message_queue::dequeue_head_n_i(message_block *&items, int number)
{
  if (this->head_ == 0) return 0;
  int count = 0;
  if (number == -1)  // dequeue all
  {
    items = this->head_;
    this->head_ = this->tail_ = 0;
    count = this->mb_count_;
    this->mb_count_ = 0;
  }else
  {
    items = this->head_;
    while (number-- > 0 && this->head_ != 0)
    {
      this->head_ = this->head_->next();
      ++count;
    }
    if (this->head_ == 0)
    {
      this->tail_ = 0;
      this->mb_count_ = 0;
    }else
    {
      this->head_->prev()->next(0); // the items's tail 
      this->head_->prev(0);  // the prev pointer of the first 
      // message block must point to 0
      this->mb_count_ -= count;
    }
  }
  return count;
}
int message_queue::dequeue_tail(message_block *&last_item, 
                                const time_value *timeout/* = 0*/)
{
  int num = 0;
  ndk::guard<mtx_t> g(this->queue_mutex_);
  if (this->wait_not_empty_cond(timeout) != 0)
    return -1;
  num = this->dequeue_tail_i(last_item);
  this->not_full_cond_.signal();
  return num;
}
int message_queue::dequeue_tail_i(message_block *&last_item)
{
  if (this->tail_ == 0) return -1;  // check empty !!

  last_item = this->tail_;
  if (this->tail_->prev() == 0) // only one mb
  {
    this->head_ = this->tail_ = 0;
  }else
  {
    this->tail_->prev()->next(0);  // set eof
    this->tail_ = this->tail_->prev();  // 
  }

  --this->mb_count_;
  // clean 
  last_item->prev(0);
  last_item->next(0);
  return 0;
}
int message_queue::dequeue_tail_n(message_block *&items, 
                                  int number/* = -1*/, 
                                  const time_value *timeout/* = 0*/)
{
  if (number == 0) return 0;  // must check at first
  ndk::guard<mtx_t> g(this->queue_mutex_);
  if (this->wait_not_empty_cond(timeout) != 0)
    return -1;
  number = this->dequeue_tail_n_i(items, number);
  this->not_full_cond_.signal();
  return number;
}
int message_queue::dequeue_tail_n_i(message_block *&items, int number)
{
  if (this->tail_ == 0) return 0;  // check empty !!
  int count = 0;
  if (number == -1)  // dequeue all
  {
    items = this->head_;
    this->head_ = this->tail_ = 0;
    count = this->mb_count_;
    this->mb_count_ = 0;
  }else
  {
    while (number-- > 0 && this->tail_ != 0)
    {
      this->tail_ = this->tail_->prev();
      ++count;
    }
    if (this->tail_ == 0) // not enough
    {
      items = this->head_;
      this->head_ = 0;
      count = this->mb_count_;
      this->mb_count_ = 0;
    }else
    {
      items = this->tail_->next();
      items->prev(0);
      this->tail_->next(0); // the items's tail 
      this->mb_count_ -= count;
    }
  }
  return count;
}
} // namespace ndk

