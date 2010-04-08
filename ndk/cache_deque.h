// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-03-29 17:05
 */
//========================================================================

#ifndef NDK_CACHE_DEQUE_H_
#define NDK_CACHE_DEQUE_H_

#include "ndk/config.h"
#include "ndk/strace.h"
#include "ndk/cache_object.h"
#include "ndk/global_macros.h"

namespace ndk
{
  template<typename KEY>
  class cache_deque_item;
  /**
   * @class cache_deque
   * 
   * @brief
   */
  template<typename KEY>
  class cache_deque
  {
    typedef cache_deque_item<KEY> cache_deque_item_t;
  public:
    cache_deque()
    : size_(0),
    free_queue_(0),
    deque_head_(0),
    deque_tail_(0)
    {
      STRACE("");
    }

    ~cache_deque()
    {
      STRACE("");
      cache_deque_item_t *itor = this->deque_head_;
      while (itor)
      {
        this->release_item(itor);
        itor = itor->next();
      }
      this->deque_head_ = 0;
      this->deque_tail_ = 0;
      itor = this->free_queue_;
      while (itor)
      {
        cache_deque_item_t *p = itor;
        itor = itor->next();
        delete p;
      }
      this->free_queue_ = 0;
    }

    //
    inline bool is_empty(void) const
    { return this->size_ == 0; }

    inline bool is_full(void) const
    { return false; }

    inline int size(void) const
    { return this->size_; }

    /**
     * The new element is inserted into the end of the queue.
     */
    inline int insert(const KEY &key, cache_object *cobj)
    {
      STRACE("");
      cache_deque_item_t *item = this->alloc_item(key, cobj);
      if (item == 0) return -1;
      this->insert_i(item);
      ++this->size_;
      return 0;
    }

    /**
     * Remove the lowest priority element
     */
    inline int remove(KEY &key, cache_object *&cobj)
    {
      STRACE("");
      cache_deque_item_t *item = this->deque_head_;
      if (item == 0) return -1;
      key  = item->obj_id_;
      cobj = item->cobj_;
      this->remove_i(item);
      --this->size_;
      this->release_item(item); //
      return 0;
    }

    /**
     * Remove the specified item.
     */
    inline int remove(void *item)
    {
      STRACE("");
      if (item == 0) return -1;
      cache_deque_item_t *real_item = 
        reinterpret_cast<cache_deque_item_t *>(item);

      this->remove_i(real_item);
      --this->size_;
      this->release_item(item); //
      return 0;
    }

    /**
     * Adjust the order of <item>, its priority was changed.
     */
    inline int adjust(void *item)
    {
      STRACE("");
      if (item == 0) return -1;
      cache_deque_item_t *real_item = 
        reinterpret_cast<cache_deque_item_t *>(item);

      this->remove_i(real_item);
      real_item->cobj_->acquire();
      real_item->next(0);
      this->insert_i(real_item);
      return 0;
    }

    // For debug
    void check(void)
    {
      STRACE("");
      cache_deque_item_t *itor = this->deque_head_;
      for (; itor != 0 && itor->next() != 0; itor = itor->next())
      {
#if 1
        if (itor->priority() > itor->next()->priority())
          goto DUMP;
#else
        assert(itor->priority() <= itor->next()->priority());
#endif
      }
      return ;
DUMP:
      itor = this->deque_head_;
      for (; itor != 0 && itor->next() != 0; itor = itor->next())
      {
        fprintf(stderr, "cur priority = %d:%p next priority = %d:%p\n",
                itor->priority(),
                itor,
                itor->next()->priority(),
                itor->next());
      }
    }
  protected:
    void insert_i(cache_deque_item_t *item)
    {
      STRACE("");
      item->prev(this->deque_tail_);
      if (this->deque_tail_ != 0)
        this->deque_tail_->next(item);
      this->deque_tail_ = item;
      if (this->deque_head_ == 0)
        this->deque_head_ = this->deque_tail_;
    }

    void remove_i(cache_deque_item_t *item)
    {
      STRACE("");
      if (item->next() != 0 && item->prev() != 0)
      {
        item->prev()->next(item->next());
        item->next()->prev(item->prev());
      }else if (item->next() != 0) // is head
      {
        this->deque_head_ = item->next();
        this->deque_head_->prev(0);
      }else if (item->prev() != 0) // is tail
      {
        this->deque_tail_ = item->prev();
        this->deque_tail_->next(0);
      }else // just one
      {
        this->deque_head_ = this->deque_tail_ = 0;
      }
    }

    inline cache_deque_item_t *alloc_item(const KEY &key, cache_object *cobj)
    {
      STRACE("");
      if (this->free_queue_ == 0)
        return new cache_deque_item_t(key, cobj);
      cache_deque_item_t *item = this->free_queue_;
      this->free_queue_ = this->free_queue_->next();
      item->reset(key, cobj);
      return item;
    }

    inline void release_item(cache_deque_item_t *item)
    {
      STRACE("");
      if (item == 0) return ;
      item->next(this->free_queue_);
      this->free_queue_ = item;
    }
  private:
    int size_;

    cache_deque_item_t *free_queue_;
    cache_deque_item_t *deque_head_;
    cache_deque_item_t *deque_tail_;
  };
  /**
   * @class cache_deque_item
   * 
   * @brief
   */
  template<typename KEY>
  class cache_deque_item
  {
    friend class cache_deque<KEY>;
  public:
    cache_deque_item(const KEY &key, cache_object *obj)
      : obj_id_(key),
      cobj_(obj),
      prev_(0),
      next_(0)
    {  this->cobj_->proxy(this); }

    inline void reset(const KEY &key, cache_object *obj)
    {
      this->obj_id_ = key;
      this->cobj_   = obj;
      this->cobj_->proxy(this);
      this->next_ = 0;
      this->prev_ = 0;
    }

    inline bool operator <  (const cache_deque_item<KEY> &i)
    { return this->priority() < i.priority(); }

    inline bool operator <= (const cache_deque_item<KEY> &i)
    { return this->priority() <= i.priority(); }

    inline bool operator >  (const cache_deque_item<KEY> &i)
    { return this->priority() > i.priority(); }

    inline bool operator >= (const cache_deque_item<KEY> &i)
    { return this->priority() >= i.priority(); }

    // 
    inline int priority(void)
    { return this->cobj_->priority(); }

    inline int priority(void) const
    { return this->cobj_->priority(); }

    inline cache_deque_item<KEY> *next()
    { return this->next_; }

    inline void next(cache_deque_item<KEY> *val)
    { this->next_ = val;  }

    inline cache_deque_item<KEY> *prev()
    { return this->prev_; }

    inline void prev(cache_deque_item<KEY> *val)
    { this->prev_ = val;  }

  protected:
    KEY  obj_id_;
    cache_object *cobj_;
    cache_deque_item<KEY> *prev_;
    cache_deque_item<KEY> *next_;
  };
} // namespace ndk

#endif // NDK_CACHE_DEQUE_H_

