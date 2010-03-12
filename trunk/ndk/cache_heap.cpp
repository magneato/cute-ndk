#ifndef NDK_CACHE_HEAP_CPP_
#define NDK_CACHE_HEAP_CPP_

#include "ndk/strace.h"
#include "ndk/cache_heap.h"

#include <cassert>
#include <cstdio>

namespace ndk
{
template<typename KEY>
cache_heap<KEY>::cache_heap(int max_size)
: max_size_(max_size)
, size_(0)
, free_heap_items_(0)
, heap_(0)
{
  STRACE("");
  heap_ = new cache_heap_item_t *[this->max_size_];
  for (int i = 0; i < this->max_size_; ++i)
    heap_[i] = 0;
}
template<typename KEY>
cache_heap<KEY>::~cache_heap()
{
  STRACE("");
  for (int i = 0; i < this->size_; ++i)
    delete this->heap_[i];

  delete []this->heap_;
  this->heap_ = 0;

  while (this->free_heap_items_)
  {
    cache_heap_item_t *item = this->free_heap_items_;
    this->free_heap_items_ = item->next();
    delete item;
  }
  this->free_heap_items_ = 0;
  this->size_ = 0; 
}
template<typename KEY>
inline typename cache_heap<KEY>::cache_heap_item_t *cache_heap<KEY>::alloc_item(const KEY &key, 
                                                                                cache_object *cobj)
{
  STRACE("");
  if (this->free_heap_items_ == 0)
    return new cache_heap_item_t(key, cobj);
  cache_heap_item_t *item = this->free_heap_items_;
  this->free_heap_items_ = this->free_heap_items_->next();
  item->reset(key, cobj);
  return item;
}
template<typename KEY>
inline void cache_heap<KEY>::release_item(cache_heap_item_t *item)
{
  STRACE("");
  if (item == 0) return ;
  item->heap_idx_ = -1;
  item->next(this->free_heap_items_);
  this->free_heap_items_ = item;
  return ;
}
template<typename KEY>
int cache_heap<KEY>::adjust(void *item)
{
  if (item == 0) return 0;
  cache_heap_item_t *real_item = reinterpret_cast<cache_heap_item_t *>(item);

  assert(this->heap_[real_item->heap_idx_] == real_item);
  if (this->heap_[real_item->heap_idx_] != real_item)
    return -1;
  this->remove_i(real_item->heap_idx_);
  //
  return this->insert_i(real_item);
}
template<typename KEY>
void cache_heap<KEY>::shift_up(int pos)
{
  STRACE("");
  cache_heap_item_t *item = this->heap_[pos];

  int lchild = (pos * 2) + 1;
  while (lchild < this->size_)
  {
    if ((lchild + 1) < this->size_ 
        && *(this->heap_[lchild]) > *(this->heap_[lchild + 1]))
      ++lchild;
    if (*item <= *(this->heap_[lchild]))
      break;
    else
    {
      this->heap_[pos] = this->heap_[lchild];
      this->heap_[pos]->heap_idx_ = pos;
      pos = lchild;
      lchild = (2 * lchild) + 1;
    }
  }
  this->heap_[pos] = item;
  this->heap_[pos]->heap_idx_ = pos;
}
template<typename KEY>
void cache_heap<KEY>::shift_down(int pos)
{
  STRACE("");
  cache_heap_item_t *item = this->heap_[pos];

  int parent = (pos - 1) / 2;
  while (pos > 0)
  {
    if (*(this->heap_[parent]) <= *item)
      break;
    else
    {
      this->heap_[pos] = this->heap_[parent];
      this->heap_[pos]->heap_idx_ = pos;
      pos = parent;
      parent = (parent - 1) / 2;
    }
  }
  this->heap_[pos] = item;
  this->heap_[pos]->heap_idx_ = pos;
}
template<typename KEY>
inline int cache_heap<KEY>::insert(const KEY &key, 
                                   cache_object *cobj)
{
  STRACE("");
  if (this->is_full()) return -1;

  //
  cache_heap_item_t *item = this->alloc_item(key, cobj);
  if (item == 0) return -1;

  return this->insert_i(item);
}
template<typename KEY>
inline int cache_heap<KEY>::insert_i(cache_heap_item_t *item)
{
  //
  this->heap_[this->size_] = item;
  this->shift_down(this->size_);
  ++this->size_;

  return 0;
}
template<typename KEY>
inline typename cache_heap<KEY>::cache_heap_item_t *cache_heap<KEY>::remove_i(int pos)
{
  STRACE("");
  if (pos < 0 || pos >= this->size_) return 0;
  cache_heap_item_t *item = this->heap_[pos];
  --this->size_;
  this->heap_[pos] = this->heap_[this->size_];
  int parent = (pos - 1) / 2;
  if (pos == 0) parent = 0;
  if (*(this->heap_[this->size_]) >= *(this->heap_[parent]))
    this->shift_up(pos);
  else
    this->shift_down(pos);
  return item;
}
template<typename KEY>
inline int cache_heap<KEY>::remove(KEY &key, cache_object *&cobj)
{
  STRACE("");
  if (this->is_empty()) return -1;

  cache_heap_item_t *item = this->heap_[0];
  this->remove_i(0);

  key  = item->obj_id_;
  cobj = item->cobj_;

  this->release_item(item); //
  return 0;
}
template<typename KEY>
int cache_heap<KEY>::remove(void *item)
{
  STRACE("");
  assert(!this->is_empty());
  if (this->is_empty() || item == 0) return -1;

  cache_heap_item_t *real_item = 
    reinterpret_cast<cache_heap_item_t *>(item);

  assert(this->heap_[real_item->heap_idx_] == real_item);
  if (this->heap_[real_item->heap_idx_] != real_item)
    return -1;

  real_item->cobj_->heap_item(0);
  this->remove_i(real_item->heap_idx_);

  this->release_item(real_item); //
  return 0;
}
template<typename KEY>
void cache_heap<KEY>::dump()
{
  fprintf(stderr, "cache heap elements: ");
  for (int i = 0; i < this->size_; ++i)
  {
    fprintf(stderr, "%d:%d:%p ", 
            i, 
            this->heap_[i]->priority(),
            this->heap_[i]);
  }
  fprintf(stderr, "\n");
  fprintf(stderr, "cache heap sorted elements: ");
  cache_heap_item_t *item = 0;
  int i = 0;
  while (1)
  {
    item = this->remove_i(0);
    if (item == 0) break;
    fprintf(stderr, "%d:%d:%p ", 
            i++, 
            item->priority(),
            item);
  }
  fprintf(stderr, "\n");
}
} // namespace ndk

#endif // NDK_CACHE_HEAP_CPP_
