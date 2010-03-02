// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-02-28 12:21
 */
//========================================================================

#ifndef NDK_CACHE_HEAP_H_
#define NDK_CACHE_HEAP_H_

namespace ndk
{
  /**
   * @class cache_heap
   * 
   * @brief
   */
  template<typename KEY>
  class cache_heap
  {
  public:
    cache_heap()
    { }

    ~cache_heap()
    { }

    //
    int is_empty (void) const
    { return this->size_ == 0; }
    int is_full (void) const
    { return this->size_ == this->max_size_; }

    //
    size_t size (void) const;

    int insert (const KEY &key, cache_object *const &cobj);

    int remove (KEY &key, cache_object *&cobj); 

    int remove (void *item);

    int adjust (void *item);
  protected:
    // insert item into heap.
    void insert_i (cache_heap_item *item);

    // remove the element residing at pos, but do not delete it.
    void remove_i (size_t pos);

    // remove the element residing at the top of heap, but do not delete it.
    void remove_i (void);
  };

  /**
   * @class cache_heap_item
   * 
   * @brief
   */
  template<typename KEY>
  class cache_heap_item
  {
  public:
    cache_heap_item(const KEY &key, cache_object *obj)
      : heap_idx_(0),
      obj_id_(key),
      cobj_(obj)
    {  }
    // 
    inline int priority (void)
    { return this->cobj_->priority(); }
  protected:
    int  heap_idx_;

    KEY  obj_id_;
    cache_object *cobj_;
  };
} // namespace ndk

#endif // NDK_CACHE_HEAP_H_

