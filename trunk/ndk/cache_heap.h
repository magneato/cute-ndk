// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-02-28 12:21
 */
//========================================================================

#ifndef NDK_CACHE_HEAP_H_
#define NDK_CACHE_HEAP_H_

#include "ndk/cache_object.h"

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

    cache_heap(int max_size);

    ~cache_heap();

    //
    int is_empty(void) const
    { return this->size_ == 0; }

    int is_full(void) const
    { return this->size_ == this->max_size_; }

    //
    int size(void) const;

    int insert(const KEY &key, cache_object *cobj);

    int remove(KEY &key, cache_object *&cobj); 

    int remove(void *item);

  protected:
    cache_heap_item_t *alloc_item(const KEY &, cache_object *);

    void release_item(cache_heap_item_t *);

    void shift_down(int pos);

    //
    void shift_up(int pos);

    // remove the element residing at pos, but do not delete it.
    cache_heap_item_t *remove_i(int pos);
  private:
    int max_size_;

    // current items.
    int size_;

    cache_heap_item_t *free_heap_items_;

    cache_heap_item_t **heap_;
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
      : heap_idx_(0),
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
  protected:
    int  heap_idx_;

    KEY  obj_id_;
    cache_object *cobj_;
    cache_heap_item<KEY> *next_;
  };
} // namespace ndk
#include "ndk/cache_heap.cpp"
#endif // NDK_CACHE_HEAP_H_

