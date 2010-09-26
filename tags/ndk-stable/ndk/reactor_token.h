// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-10 23:13
 */
//========================================================================

#ifndef NDK_REACTOR_TOKEN_H_
#define NDK_REACTOR_TOKEN_H_

#include "ndk/token.h"
#include "ndk/unused_arg.h"
#include "ndk/reactor_impl.h"

namespace ndk
{
  // Forward declarations
  //

  /**
   * @class reactor_token
   * 
   * @brief 
   */
  class reactor_token : public token
  {
  public:
    reactor_token(reactor_impl *r_impl,
                  int s_queue = token::FIFO)
    : token (s_queue),
    reactor_(r_impl)
    { }

    virtual ~reactor_token()
    { }

    inline reactor_impl *reactor()
    {
      return this->reactor_;
    }

    inline void reactor(reactor_impl *r)
    {
      this->reactor_ = r;
    }

    inline virtual void sleep_hook(void)
    {
      if (this->reactor_)
        this->reactor_->notify(0, 0);
    }
  private:
    reactor_impl *reactor_;
  };
  /**
   * @class reactor_null_token
   * 
   * @brief 
   */
  class reactor_null_token : public token
  {
  public:
    reactor_null_token(reactor_impl *r_impl,
                       int s_queue = token::FIFO)
    : token (s_queue),
    reactor_(r_impl)
    { }

    virtual ~reactor_null_token() { }

    inline reactor_impl *reactor()
    { return this->reactor_; }

    inline void reactor(reactor_impl *r)
    { this->reactor_ = r; }

    inline virtual void sleep_hook(void) { }

    inline virtual int acquire(void(*fun)(void *),
                               void *,
                               const time_value *)
    { unused_args(fun); return 0; }

    inline virtual int acquire(const time_value *) { return 0; }

    inline virtual int acquire_read(void(*fun)(void *),
                                    void *,
                                    const time_value *)
    { unused_args(fun); return 0; }

    inline virtual int release(void) { return 0; }

    inline virtual int try_acquire(void) { return 0; }
  private:
    reactor_impl *reactor_;
  };
  /**
   * @class token_guard
   * @brief A helper class that helps grabbing, releasing and waiting
   * on tokens for a thread that needs access to the reactor's token.
   */
  static void polite_sleep_hook(void *) { }
  class token_guard
  {
  public:
    // Constructor that will grab the token for us
    token_guard(reactor_token &tk)
      : token_(tk),
      owner_(0)
    { }

    // Destructor. This will release the token if it hasn't been
    // released till this point
    ~token_guard(void) { this->release_token(); }

    // Release the token ..
    void release_token(void)
    {
      STRACE("");
      if (this->owner_ == 1)
      { this->token_.release(); this->owner_ = 0; }
    }

    // Returns whether the thread that created this object owns the
    // token or not.
    int is_owner(void) { return this->owner_;   }

    // A helper method that acquires the token 1) at a low priority
    // and wait quietly for the token, not waking another thread. This
    // is appropriate for cases where a thread wants to wait for and
    // dispatch an event, not causing an existing waiter to relinquish the
    // token, and also queueing up behind other threads waiting to modify
    // event records.
    int acquire_quietly(const time_value *timeout = 0)
    {
      STRACE("");
      int result = 0;
      if (timeout == 0)
        result = this->token_.acquire_read(&polite_sleep_hook);
      else
        result = this->token_.acquire_read(&polite_sleep_hook,
                                           0,
                                           timeout);
      if (result == 0) this->owner_ = 1;
      return result;
    }

    // A helper method that acquires the token at a high priority, and
    // does wake the current token holder.
    int acquire(const time_value *timeout = 0)
    {
      int result = 0;
      if (timeout == 0) result = this->token_.acquire();
      else result = this->token_.acquire(0, 0, timeout);

      if (result == 0) this->owner_ = 1;
      return result;
    }
  private:
    // The Reactor token.
    reactor_token &token_;

    // Flag that indicate whether the thread that created this object
    // owns the token or not. A value of 0 indicates that this class
    // hasn't got the token(and hence the thread) and a value of 1
    // vice-versa.
    volatile int owner_;
  };
} // namespace ndk

#endif // NDK_REACTOR_TOKEN_H_

