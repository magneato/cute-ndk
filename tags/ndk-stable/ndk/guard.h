// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 00:10
 */
//========================================================================

#ifndef NDK_GUARD_H_
#define NDK_GUARD_H_

namespace ndk
{
  /**
   * @class guard
   *
   * @brief scoped auto lock
   */
  template<typename lock>
  class guard
  {
  public:
    inline guard(lock &l)
      : mutex_(l)
    {  this->mutex_.acquire(); }

    inline guard(const lock &l)
      : mutex_(const_cast<lock &>(l))
    {  this->mutex_.acquire(); }
      
    inline ~guard()
    { this->mutex_.release(); }

    inline void acquire(void)
    { this->mutex_.acquire(); }
      
    inline void release(void)
    { this->mutex_.release(); }

    inline int try_acquire(void)
    { return this->mutex_.try_acquire(); }
  private:
    lock &mutex_;
  };
  
  /**
   * @class guard for null_mutex
   *
   * @brief scoped auto lock
   */
  class null_thread;
  template<>
  class guard<null_thread>
  {
  public:
    inline guard(null_thread &) { }

    inline guard(const null_thread &) {  }
      
    inline ~guard() { }

    inline void acquire(void) { }
      
    inline void release(void) { }

    inline int try_acquire(void) { return 0; }
  };

} // namespace ndk
#endif // NDK_GUARD_H_

