#ifndef NDK_CACHE_MANAGER_CPP_
#define NDK_CACHE_MANAGER_CPP_

#include "ndk/guard.h"
#include "ndk/cache_manager.h"

namespace ndk
{
template<typename KEY, typename SYNCH_MUTEX>
cache_manager<KEY, SYNCH_MUTEX>::cache_manager(int max_size,
                                               int min_obj_size,
                                               int max_obj_size,
                                               int high_water_mark,
                                               int low_water_mark,
                                               cache_object_factory *cof)
: cobj_min_size_(min_obj_size),
  cobj_max_size_(max_obj_size),
  water_mark_(0),
  high_water_mark_(high_water_mark * 1024 * 1024),
  low_water_mark_(low_water_mark * 1024 * 1024),
  factory_(cof)
{
  if (factory_ == 0)
    factory_ = new lru_cache_object_factory();
  cache_heap_ = new cache_heap<KEY>(1024);
}
template<typename KEY, typename SYNCH_MUTEX>
cache_manager<KEY, SYNCH_MUTEX>::~cache_manager()
{
  if (this->cache_heap_)
    delete this->cache_heap_;
  this->cache_heap_ = 0;
}
template<typename KEY, typename SYNCH_MUTEX>
inline cache_object *cache_manager<KEY, SYNCH_MUTEX>::get(const KEY &key)
{
  guard<SYNCH_MUTEX> g(this->cache_mutex_);
  return this->get_i(key);
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_manager<KEY, SYNCH_MUTEX>::put(const KEY &key, 
                                                void *data,
                                                size_t size,
                                                cache_object_observer *ob,
                                                cache_object *&cobj)
{
  guard<SYNCH_MUTEX> g(this->cache_mutex_);
  return this->put_i(key, data, size, ob, cobj);
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_manager<KEY, SYNCH_MUTEX>::drop(cache_object *&cobj)
{
  guard<SYNCH_MUTEX> g(this->cache_mutex_);
  return this->drop_i(cobj);
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_manager<KEY, SYNCH_MUTEX>::flush(void)
{
  guard<SYNCH_MUTEX> g(this->cache_mutex_);
  return this->flush_i();
}
template<typename KEY, typename SYNCH_MUTEX>
inline cache_object *cache_manager<KEY, SYNCH_MUTEX>::get_i(const KEY &key)
{
  cache_map_itor itor = this->cache_map_.find(key);
  if (itor == this->cache_map_.end())
    return 0;
  itor->second->acquire();
  return itor->second;
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_manager<KEY, SYNCH_MUTEX>::put_i(const KEY &key, 
                                                  void *data,
                                                  size_t size,
                                                  cache_object_observer *ob,
                                                  cache_object *&cobj)
{
  if (data == 0) 
  {
    this->flush_i(key); // 
    cobj = 0;
    return 0;
  }
  if (this->make_cobj(data, size, ob, cobj) == -1)
    return -1;

  std::pair<cache_map_itor, bool> ret = 
    this->cache_map_.insert(std::make_pair(key, cobj));
  if (!ret.second) // insert failed. the pair was actually inserted
  {
#ifdef NDK_RTLOG
    fprintf(stderr, 
            "warning: insert failed, the pair was actually inserted! [%s:%d]\n",
            __FILE__,
            __LINE__);
#endif
    cache_object *old_obj = ret.first->second;

    this->cache_heap_->remove(old_obj->heap_item()); // remove from heap.
    this->cache_map_[key] = cobj; // update

    this->water_mark_ -= old_obj->size();
    this->drop_i(old_obj);
  }
  if (this->cache_heap_->insert(key, cobj) != 0)
  {
#ifdef NDK_RTLOG
    fprintf(stderr, 
            "insert to heap failed![%s:%d]\n",
            __FILE__,
            __LINE__);
#endif
    this->cache_map_.erase(key);
    this->drop_i(cobj);
    return -1;
  }

  this->water_mark_ += cobj->size();
  return 0;
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_manager<KEY, SYNCH_MUTEX>::drop_i(cache_object *&cobj)
{
  cobj->release();
  this->factory_->destroy(cobj);
  cobj = 0;
  return 0;
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_manager<KEY, SYNCH_MUTEX>::make_cobj(void *data, 
                                                      size_t size, 
                                                      cache_object_observer *ob,
                                                      cache_object *&obj)
{
  if (size > this->cobj_max_size_
      || size < this->cobj_min_size_) // 
  {
#ifdef NDK_RTLOG
    fprintf(stderr, 
            "error: [%u] is too small/large to cache\n", 
            size);
#endif
    return -1;
  }
  if (size + this->water_mark_ > this->high_water_mark_)
  {
    // drop some object.
    do
    {
      if (this->flush_i() == -1)
      {
#ifdef NDK_RTLOG
        fprintf(stderr, "error: flush error!\n");
#endif
        return -1;
      }
    }while (this->water_mark_ > this->low_water_mark_);
  }

  // make sure heap has enough space. 
  if (this->cache_heap_->is_full())
  {
#ifdef NDK_RTLOG
    fprintf(stderr, "heap is full, flushing...\n");
#endif
    if (this->flush_i() == -1)
    {
#ifdef NDK_RTLOG
      fprintf(stderr, "flush error![%s:%d]\n", __FILE__, __LINE__);
#endif
      return -1;
    }
  }
  obj = this->factory_->create(data, size, ob);
  obj->acquire();
  return 0;
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_manager<KEY, SYNCH_MUTEX>::flush_i(void)
{
  KEY temp_key;
  cache_object *temp_obj = 0;
  if (this->cache_heap_->remove(temp_key, temp_obj) == 0)
  {
    this->cache_map_.erase(temp_key);
    this->water_mark_ -= temp_obj->size();
    temp_obj->release();
    this->drop_i(temp_obj);
  }else
    return -1;
  return 0;
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_manager<KEY, SYNCH_MUTEX>::flush_i(const KEY &key)
{
  cache_map_itor itor = this->cache_map_.find(key);
  if (itor == this->cache_map_.end())
    return -1;
  cache_object *temp_obj = itor->second;
  this->cache_map_.erase(itor);

  this->cache_heap_->remove(temp_obj->heap_item());
  this->water_mark_ -= temp_obj->size();
  this->drop_i(temp_obj);
  return 0;
}
} // namespace ndk
#endif // NDK_CACHE_MANAGER_CPP_
