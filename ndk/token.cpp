#include "ndk/token.h"
#include "ndk/strace.h"

#include <cassert>

namespace ndk
{
int token::shared_acquire(void(*sleep_hook_func)(void *),
                          void *arg,
                          const time_value * timeout,
                          token_opt_type opt_type)
{
  STRACE("");
  guard<thread_mutex> g(this->lock_);

  thread_t thr_id = thread::self();
  if (!this->in_use_)
  {
    this->in_use_ = opt_type;
    this->owner_  = thr_id;
    return 0;
  }

  // Someone already holds the lock
  if (thread::thr_equal(thr_id, this->owner_))
  {
    ++this->nesting_level_;
    return 0;
  }

  //
  // We've got to sleep until we get the token.
  //

  token_queue *queue = (opt_type == token::read_token 
                        ? &this->readers_
                        : &this->writers_);
  // Allocate queue entry on stack.  This works since we don't exit
  // this method's activation record until we've got the token.
  token::token_queue_entry my_entry(this->lock_,
                                    thr_id);
  queue->insert_entry(my_entry, this->queueing_strategy_);

  ++this->waiters_;

  if (sleep_hook_func)
  {
    (*sleep_hook_func)(arg);
  }else
  {
    this->sleep_hook();
  }

  // Sleep until we've got the token
  int time_out = 0;
  int error = 0;
  do
  {
    int result = my_entry.wait(timeout);
    if (result == -1)
    {
      if (errno == EINTR)
        continue;
      if (errno == ETIMEDOUT)
        time_out = 1;
      else
        error = 1;
      this->wakeup_next_waiter();
      break;
    }
  }while (!thread::thr_equal(thr_id, this->owner_));
  // Do this always and irrespective of the result of wait().
  --this->waiters_;
  queue->remove_entry(&my_entry);

  if (time_out)
  {
    // This thread was still selected to own the token.
    if (my_entry.runable_)
    {
      // Wakeup next waiter since this thread timed out.
      this->wakeup_next_waiter();
    }
    return -1;
  }else if (error == 1)
    return -1;
  // If this is a normal wakeup, this thread should be runnable.
  assert(my_entry.runable_);
  return 0;
}
void token::wakeup_next_waiter()
{
  STRACE("");
  this->in_use_ = 0;
  this->owner_  = NULL_thread;

  // Any waiters ...
  if (this->writers_.head_ == 0 
      && this->readers_.head_ == 0)
  {
    // No more waiters
    return ;
  }
  // Wakeup next waiter
  token_queue *queue = 0;
  // Writer threads get priority to run first.
  if (this->writers_.head_ != 0)
  {
    this->in_use_ = token::write_token;
    queue = &this->writers_;
  }else
  {
    this->in_use_ = token::read_token;
    queue = &this->readers_;
  }
  // Wake up waiter and make it runable.
  queue->head_->runable_ = 1;
  this->owner_ = queue->head_->thread_id_;
  queue->head_->signal();
}
//-----------------------------------------------------------------
void token::token_queue::remove_entry(token::token_queue_entry *entry)
{
  STRACE("token::token_queue");
  token::token_queue_entry *curr = 0;
  token::token_queue_entry *prev = 0;

  if (this->head_ == 0)
    return ;
  for (curr = this->head_; curr != 0 && curr != entry; curr = curr->next_)
    prev = curr;
  if (curr == 0) // Didn't find the entry...
    return ;
  else if (prev == 0) // Delete at the head
    this->head_ = this->head_->next_;
  else  // Delete in the middle.
    prev->next_ = curr->next_;
  // We need to update the tail of the list if we've deleted the last
  // entry.
  if (curr->next_ == 0)
    this->tail_ = prev;
}
void token::token_queue::insert_entry(token::token_queue_entry &entry,
                                      int queue_strategy/* = FIFO*/)
{
  STRACE("");
  if (this->head_ == 0)
  {
    // No other threads - just add me
    this->head_ = &entry;
    this->tail_ = &entry;
  }else if (queue_strategy == LIFO)
  {
    // Insert at head of queue
    entry.next_ = this->head_;
    this->head_ = &entry;
  }else // if (queue_strategy == FIFO)
  {
    // Insert at the end of the queue.
    this->tail_->next_ = &entry;
    this->tail_ = &entry;
  }
}
} // namespace ndk

