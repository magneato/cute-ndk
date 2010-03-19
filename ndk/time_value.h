// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 02:24
 */
//========================================================================

#ifndef NDK_TIME_VALUE_H_
#define NDK_TIME_VALUE_H_

#include <sys/time.h>

namespace ndk
{
  /**
   * @class time_value
   *
   * @brief 
   */
  class time_value
  {
  public:
    // Constant "0".
    static const time_value zero;

    static const time_value start_time;
    
    time_value() { set(0, 0); }

    time_value(const time_value& tv)
    { set(tv.tv_.tv_sec, tv.tv_.tv_usec); }
    
    explicit time_value(time_t sec, long int usec = 0)
    { set(sec, usec); }

    // construct the time_value from a timeval.
    explicit time_value(const timeval &tv) { set(tv); }

    // construct the time_value object from a timespec_t.
    explicit time_value(const timespec &tv) { set(tv); }

    inline time_value &operator = (const time_value &tv)
    {
      this->tv_.tv_sec  = tv.tv_.tv_sec;
      this->tv_.tv_usec = tv.tv_.tv_usec;
      return *this;
    }

    inline time_value &operator = (time_t tv)
    {
      this->tv_.tv_sec  = tv;
      this->tv_.tv_usec = 0;
      return *this;
    }

    // initializes the time_value from seconds and useconds.
    inline void set (time_t sec, long int usec)
    {
      this->tv_.tv_sec  = sec;
      this->tv_.tv_usec = usec;
      this->normalize();
    }

    // initializes the time_value from a timeval.
    inline void set(const timeval &tv)
    { this->set(tv.tv_sec, tv.tv_usec); }

    // initializes the time_value object from a timespec_t.
    inline void set(const timespec &t)
    { this->set(t.tv_sec, t.tv_nsec/1000); }

    inline unsigned long msec (void) const
    { return this->tv_.tv_sec * 1000 + this->tv_.tv_usec / 1000; }

    // returns the value of the object as a timespec_t.
    inline operator timespec () const
    {
      timespec tv;
      tv.tv_sec = this->tv_.tv_sec;
      tv.tv_nsec = this->tv_.tv_usec * 1000;
      return tv;
    }

    // returns the value of the object as a timeval.
    inline operator timeval () const
    { return this->tv_; }

    // returns a pointer to the object as a timeval.
    inline operator const timeval *() const
    { return &this->tv_; }

    // = The following are accessor/mutator methods.
    inline time_t sec(void) const
    { return this->tv_.tv_sec; }

    inline void sec(time_t sec)
    { this->tv_.tv_sec = sec; }

    inline long int usec(void) const
    { return this->tv_.tv_usec; }

    inline void usec(long int usec)
    { this->tv_.tv_usec = usec; }

    // Get current time value
    inline void update()
    { ::gettimeofday (&this->tv_, 0); }

    inline time_value &operator += (const time_value &tv)
    {
      this->tv_.tv_sec  += tv.tv_.tv_sec;
      this->tv_.tv_usec += tv.tv_.tv_usec;
      this->normalize();
      return *this;
    }

    inline time_value &operator += (const time_t tv)
    {
      this->tv_.tv_sec += tv;
      return *this;
    }

    inline time_value &operator -= (const time_value &tv)
    {
      this->tv_.tv_sec  -= tv.tv_.tv_sec;
      this->tv_.tv_usec -= tv.tv_.tv_usec;
      this->normalize();
      return *this;
    }

    inline time_value &operator -= (const time_t tv)
    {
      this->tv_.tv_sec -= tv;
      return *this;
    }

    inline friend time_value operator + (const time_value &tv1, 
                                         const time_value &tv2)
    {
      time_value sum(tv1);
      sum += tv2;
      return sum;
    }

    inline friend time_value operator - (const time_value &tv1, 
                                         const time_value &tv2)
    {
      time_value sum(tv1);
      sum -= tv2;
      return sum;
    }

    inline friend bool operator < (const time_value &tv1,
                                   const time_value &tv2)
    {
      if (tv1.sec () < tv2.sec ())
        return true;
      else if (tv1.sec () == tv2.sec ()
               && tv1.usec () < tv2.usec ())
        return true;
      return false;
    }

    inline friend bool operator > (const time_value &tv1,
                                   const time_value &tv2)
    { return tv2 < tv1; }

    inline friend bool operator <= (const time_value &tv1,
                                    const time_value &tv2)
    {
      if (tv1.sec () < tv2.sec ())
        return true;
      else if (tv1.sec () == tv2.sec ()
               && tv1.usec () <= tv2.usec ())
        return true;
      return false;
    }

    inline friend bool operator >= (const time_value &tv1,
                                    const time_value &tv2)
    { return tv2 <= tv1; }

    inline friend bool operator == (const time_value &tv1,
                                    const time_value &tv2)
    {
      return tv1.sec () == tv2.sec ()
        && tv1.usec () == tv2.usec ();
    }

    inline friend bool operator != (const time_value &tv1,
                                    const time_value &tv2)
    { return !(tv1 == tv2); }
    static inline time_value gettimeofday()
    {
        struct timeval tv;
        ::gettimeofday(&tv, 0);
        return time_value(tv);
    }
  protected:
    // Put the timevalue into a canonical form.
    void normalize(void);

  private:
    struct timeval tv_;
  };
} // namespace ndk

#endif // NDK_TIME_VALUE_H_

