// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-02-27 11:51
 */
//========================================================================

#ifndef NDK_ATOMIC_OPT_H_
#define NDK_ATOMIC_OPT_H_

#include "ndk/guard.h"

namespace ndk
{
  /**
   * @class atomic_opt
   * 
   * @brief
   */
  template<typename TYPE, typename MUTEX>
  class atomic_opt
  {
  public:
    // default init data to 0
    atomic_opt()
      : data_(0)
    { }

    // init value to data
    explicit atomic_opt(const TYPE &v)
    { this->data_ = v; }

    // assgin
    inline atomic_opt<TYPE, MUTEX> &operator = (const TYPE &v)
    { 
      guard<MUTEX> g(this->mutex_);
      this->data_ = v; return *this;
    }

    //
    inline atomic_opt<TYPE, MUTEX> &operator = (const atomic_opt<TYPE, MUTEX> &v)
    {
      if (&v == this) return *this;
      guard<MUTEX> g (this->mutex_);
      this->data_ = v.value ();
      return *this;
    }

    // atomically pre-increment value_.
    inline TYPE operator ++ (void)
    {
      guard<MUTEX> g(this->mutex_); 
      return ++this->data_;
    }

    // atomically post-increment value_.
    inline TYPE operator ++ (int)
    {
      guard<MUTEX> g(this->mutex_);
      return this->data_++;
    }

    // atomically increment value_ by rhs
    inline TYPE operator += (const TYPE &rhs)
    {
      guard<MUTEX> g(this->mutex_);
      return this->data_ += rhs;
    }

    // atomically pre-decrement value_.
    inline TYPE operator -- (void)
    {
      guard<MUTEX> g(this->mutex_);
      return --this->data_;
    }

    // atomically post-decrement value_.
    inline TYPE operator -- (int)
    {
      guard<MUTEX> g(this->mutex_);
      return this->data_--;
    }

    // atomically decrement value_ by rhs.
    inline TYPE operator -= (const TYPE &rhs)
    {
      guard<MUTEX> g(this->mutex_);
      return this->data_ -= rhs;
    }

    // atomically compare value_ with rhs.
    inline bool operator == (const TYPE &rhs)
    {
      guard<MUTEX> g(this->mutex_);
      return this->data_ == rhs;
    }

    // atomically compare value_ with rhs.
    inline bool operator != (const TYPE &rhs)
    {
      guard<MUTEX> g(this->mutex_);
      return this->data_ != rhs;
    }

    // atomically check if value_ less than rhs.
    inline bool operator  < (const TYPE &rhs)
    {
      guard<MUTEX> g(this->mutex_);
      return this->data_ < rhs;
    }

    // atomically check if value_ less than or equal to rhs.
    inline bool operator <= (const TYPE &rhs)
    {
      guard<MUTEX> g(this->mutex_);
      return this->data_ <= rhs;
    }

    // atomically check if value_ greater than rhs.
    inline bool operator  > (const TYPE &rhs)
    {
      guard<MUTEX> g(this->mutex_);
      return this->data_ > rhs;
    }

    // atomically check if value_ greater than or equal to rhs.
    inline bool operator >= (const TYPE &rhs)
    {
      guard<MUTEX> g(this->mutex_);
      return this->data_ >= rhs;
    }

    // return value
    inline TYPE value (void)
    {
      guard<MUTEX> g(this->mutex_);
      return this->data_;
    }
  private:
    // copy constructor 
    atomic_opt(const atomic_opt<TYPE, MUTEX> &v);

    TYPE   data_;
    MUTEX  mutex_;
  };
} // namespace ndk

#endif // NDK_ATOMIC_OPT_H_

