#ifndef NDK_CACHE_HEAP_CPP_
#define NDK_CACHE_HEAP_CPP_

#include "ndk/cache_heap.h"

namespace ndk
{
template<typename KEY>
inline int cache_heap<KEY>::insert(const KEY &key, cache_object *const &cobj)
{
  if (this->is_full()) return -1;

  //
  cache_heap_item *item = new cache_heap_item(key, cobj);
  if (item == 0) return -1;
  this->insert_i(item);
  return 0;
}
template<typename KEY>
inline void cache_heap<KEY>::insert_i(cache_heap_item *item)
{
  int i = 0;
  for (i = this->size_ + 1; i > 1; i /= 2)
  {
    if (item->priority() > this->heap_[i/2 - 1]->priority())
      break;
    this->heap_[i-1] = this->heap_[i/2 - 1]; 
    this->heap_[i-1]->heap_idx_ = i-1;
  }
  this->heap_[i-1] = item;
  this->heap_[i-1]->heap_idx_ = i-1;
  ++(this->size_);

  return 0;
}
} // namespace ndk

#endif // NDK_CACHE_HEAP_CPP_
