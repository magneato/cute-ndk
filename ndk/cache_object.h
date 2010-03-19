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
#include "ndk/time_value.h"

#include "ndk/config.h"

namespace ndk
{
  class cache_object;
  /**
   * @class cache_object_observer
   * 
   * @brief
   */
  class cache_object_observer
  {
  public:
    virtual int drop(cache_object *) = 0;
  };

  /**
   * @class cache_object
   * 
   * @brief
   */
  class cache_object
  {
  public:
    cache_object(void *p, size_t s, cache_object_observer *ob)
      : refcount_(0),
      size_(s),
      data_(p),
      heap_item_(0),
      observer_(ob)
    { 
    }

    inline virtual ~cache_object()
    {
      this->data_ = 0; 
      this->size_ = 0;
      this->heap_item_ = 0;
    }
    //
    inline void *data(void) const
    { return this->data_; }

    inline size_t size(void) const
    { return this->size_; }

    inline cache_object_observer *observer()
    { return this->observer_; }

    inline void heap_item(void *val)
    { this->heap_item_ = val; }

    inline void *heap_item(void)
    { return this->heap_item_; }

    virtual int priority(void) const = 0;

    inline void acquire(void)
    {
      ++this->refcount_;
      this->acquire_i();
    }

    inline virtual void release(void)
    { --this->refcount_; }

    inline void drop()
    {
      if (this->observer_ && this->refcount_ == 0) 
        this->observer_->drop(this); 
    }

    inline int refcount()
    { return this->refcount_; }

  protected:
    virtual void acquire_i(void) = 0;

  protected:
    volatile int refcount_;
    size_t size_;

    void *data_;
    void *heap_item_;
    cache_object_observer *observer_;
  };
  /**
   * @class cache_object_factory
   * 
   * @brief
   */
  class cache_object_factory
  {
  public:
    virtual cache_object *create(void *, size_t , 
                                 cache_object_observer *) = 0;

    virtual void destroy(cache_object *) = 0;
  };
  /**
   * @class lru_cache_object
   * 
   * @brief
   */
  class lru_cache_object : public cache_object
  {
  public:
    lru_cache_object(void *p, size_t s, cache_object_observer *ob)
      : cache_object(p, s, ob),
      last_access_(0)
    { }

    inline virtual int priority(void )const 
    { return static_cast<int>(this->last_access_); }

    inline virtual void acquire_i(void)
    { 
      this->last_access_ = ::time(0) - time_value::start_time.sec(); 
    }
  protected:
    time_t last_access_;
  };
  /**
   * @class lru_cache_object_factory
   * 
   * @brief
   */
  class lru_cache_object_factory : public cache_object_factory
  {
  public:
    inline virtual cache_object *create(void *p, size_t s, 
                                        cache_object_observer *ob)
    { return new lru_cache_object(p, s, ob); }

    inline virtual void destroy(cache_object *p)
    { if (p) delete reinterpret_cast<lru_cache_object *>(p); }
  };
  /**
   * @class lfu_cache_object
   * 
   * @brief
   */
  class lfu_cache_object : public cache_object
  {
  public:
    lfu_cache_object(void *p, size_t s, cache_object_observer *ob)
      : cache_object(p, s, ob),
        hit_count_(0)
    { }

    inline virtual int priority(void )const 
    { return this->hit_count_; }

    inline virtual void acquire_i(void)
    { ++this->hit_count_; }

  protected:
    int hit_count_;
  };
  /**
   * @class lfu_cache_object_factory
   * 
   * @brief
   */
  class lfu_cache_object_factory : public cache_object_factory
  {
  public:
    inline virtual cache_object *create(void *p, size_t s, 
                                        cache_object_observer *ob)
    { return new lfu_cache_object(p, s, ob); }

    inline virtual void destroy(cache_object *p)
    { if (p) delete reinterpret_cast<lfu_cache_object *>(p); }
  };
  /**
   * @class fifo_cache_object
   * 
   * @brief
   */
  class fifo_cache_object : public cache_object
  {
  public:
    fifo_cache_object(void *p, size_t s, cache_object_observer *ob)
      : cache_object(p, s, ob)
    { }

    inline virtual int priority(void )const 
    { return 0; }

    inline virtual void acquire_i(void)
    { return ; }
  };
  /**
   * @class fifo_cache_object_factory
   * 
   * @brief
   */
  class fifo_cache_object_factory : public cache_object_factory
  {
  public:
    inline virtual cache_object *create(void *p, size_t s, 
                                        cache_object_observer *ob)
    { return new fifo_cache_object(p, s, ob); }

    inline virtual void destroy(cache_object *p)
    { if (p) delete reinterpret_cast<fifo_cache_object *>(p); }
  };
} // namespace ndk

#endif // NDK_CACHE_OBJECT_H_

