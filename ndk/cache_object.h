// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-02-27 17:35
 */
//========================================================================

#ifndef NDK_CACHE_OBJECT_H_
#define NDK_CACHE_OBJECT_H_

#include <time.h>

namespace ndk
{
  /**
   * @class cache_object
   * 
   * @brief
   */
  class cache_object
  {
  public:
    cache_object(const char *p, size_t s)
      : data_(p),
      size_(s)
    { 
      this->first_access_ = ::time((time_t *)0);
      this->last_access_  = this->first_access_;
    }

    inline virtual ~cache_object()
    {
      this->data_ = 0; 
      this->size_ = 0;
    }
    //
    inline const void *data(void) const
    { return this->data_; }

    inline size_t size(void) const
    { return this->size_; }

    //
    inline time_t first_access(void) const
    { return this->first_access_; }

    inline time_t last_access(void) const
    { return this->last_access_; }

    inline int priority (void) const = 0;
  protected:
    virtual 
  private:
    const void *data_;
    size_t size_;

    time_t last_access_;
    time_t first_access_;
  };
  /**
   * @class lru_cache_object
   * 
   * @brief
   */
  class lru_cache_object
  {
  public:
    lru_cache_object(const char *p, size_t s)
      : cache_object(p, s)
    { }

  };
  /**
   * @class lfu_cache_object
   * 
   * @brief
   */
  class lfu_cache_object
  {
  public:
    lfu_cache_object(const char *p, size_t s)
      : cache_object(p, s)
    { }

  };
  /**
   * @class fifo_cache_object
   * 
   * @brief
   */
  class fifo_cache_object
  {
  public:
    fifo_cache_object(const char *p, size_t s)
      : cache_object(p, s)
    { }

  };
} // namespace ndk

#endif // NDK_CACHE_OBJECT_H_

