// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-02-28 12:21
 */
//========================================================================

#ifndef NDK_CACHE_HEAP_H_
#define NDK_CACHE_HEAP_H_

#include "ndk/heap.h"
#include "ndk/config.h"
#include "ndk/cache_object.h"
#include "ndk/global_macros.h"

namespace ndk
{
  template<typename KEY>
  class cache_heap_item;
  /**
   * @class cache_heap
   * 
   * @brief
   */
  template<typename KEY>
  class cache_heap
  {
  public:
    typedef cache_heap_item<KEY> cache_heap_item_t;

    cache_heap(int min_or_max/* min = 0 max = 1*/, int max_size)
    : free_heap_items_(0),
    heap_(0)
    {
      STRACE("");
      if (min_or_max == 0/*min*/)
        heap_ = new min_heap<cache_heap_item_t>(max_size);
      else
        heap_ = new max_heap<cache_heap_item_t>(max_size);
    }

    //
    inline bool is_empty(void) const
    { return this->heap_->is_empty(); }

    inline bool is_full(void) const
    { return this->heap_->is_full(); }

    inline int size(void) const
    { return this->heap_->size(); }

    inline int insert(const KEY &key, cache_object *cobj)
    {
      STRACE("");
      if (this->heap_->is_full()) return -1;
      cache_heap_item_t *item = this->alloc_item(key, cobj);
      if (item == 0) return -1;
      return this->heap_->push(item);
    }

    inline int remove(KEY &key, cache_object *&cobj)
    {
      STRACE("");
      cache_heap_item_t *item = this->heap_->pop(0);
      if (item == 0) return -1;
      key  = item->obj_id_;
      cobj = item->cobj_;
      this->release_item(item); //
      return 0;
    }

    inline int remove(void *item)
    {
      STRACE("");
      if (item == 0) return -1;
      cache_heap_item_t *real_item = 
        reinterpret_cast<cache_heap_item_t *>(item);

      real_item->cobj_->heap_item(0);
      if (this->heap_->erase(real_item) == 0)
      {
        this->release_item(real_item); //
        return 0;
      }
      return -1;
    }

    //
    inline int adjust(void *item)
    {
      STRACE("");
      if (item == 0) return -1;
      cache_heap_item_t *real_item = 
        reinterpret_cast<cache_heap_item_t *>(item);
      //return this->heap_->adjust(real_item);
      this->heap_->erase(real_item);
      real_item->cobj_->acquire();
      return this->heap_->push(real_item);
    }

    void check_heap(void)
    {
      STRACE("");
      this->heap_->check_heap();
    }

    // release 
    ~cache_heap()
    {
      STRACE("");
      cache_heap_item_t *item = 0;
      do
      {
        item = this->heap_->pop();
        this->release_item(item);
      }while (item != 0);

      while (this->free_heap_items_)
      {
        cache_heap_item_t *item = this->free_heap_items_;
        this->free_heap_items_ = item->next();
        delete item;
      }
      this->free_heap_items_ = 0;

      if (this->heap_)
        delete this->heap_;
      this->heap_ = 0;
    }

  protected:
    inline cache_heap_item_t *alloc_item(const KEY &key, cache_object *cobj)
    {
      STRACE("");
      if (this->free_heap_items_ == 0)
        return new cache_heap_item_t(key, cobj);
      cache_heap_item_t *item = this->free_heap_items_;
      this->free_heap_items_ = this->free_heap_items_->next();
      item->reset(key, cobj);
      return item;
    }

    inline void release_item(cache_heap_item_t *item)
    {
      STRACE("");
      if (item == 0) return ;
      item->heap_idx_ = -1;
      item->next(this->free_heap_items_);
      this->free_heap_items_ = item;
    }
  private:
    cache_heap_item_t *free_heap_items_;

    heap<cache_heap_item_t> *heap_;
  };

  /**
   * @class cache_heap_item
   * 
   * @brief
   */
  template<typename KEY>
  class cache_heap_item
  {
    friend class cache_heap<KEY>;
  public:
    cache_heap_item(const KEY &key, cache_object *obj)
      : heap_idx_(-1),
      obj_id_(key),
      cobj_(obj),
      next_(0)
    {  this->cobj_->heap_item(this); }

    inline void reset(const KEY &key, cache_object *obj)
    {
      this->obj_id_ = key;
      this->cobj_   = obj;
      this->cobj_->heap_item(this);
      this->next_ = 0;
      this->heap_idx_ = -1;
    }

    inline bool operator <  (const cache_heap_item<KEY> &i)
    { return this->priority() < i.priority(); }

    inline bool operator <= (const cache_heap_item<KEY> &i)
    { return this->priority() <= i.priority(); }

    inline bool operator >  (const cache_heap_item<KEY> &i)
    { return this->priority() > i.priority(); }

    inline bool operator >= (const cache_heap_item<KEY> &i)
    { return this->priority() >= i.priority(); }

    // 
    inline int priority(void)
    { return this->cobj_->priority(); }

    inline int priority(void) const
    { return this->cobj_->priority(); }

    inline cache_heap_item<KEY> *next()
    { return this->next_; }

    inline void next(cache_heap_item<KEY> *val)
    { this->next_ = val;  }
  public:
    int  heap_idx_;

  protected:
    KEY  obj_id_;
    cache_object *cobj_;
    cache_heap_item<KEY> *next_;
  };
} // namespace ndk
#endif // NDK_CACHE_HEAP_H_

