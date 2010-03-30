#ifndef NDK_CACHE_MANAGER_CPP_
#define NDK_CACHE_MANAGER_CPP_

#include "ndk/cache_manager.h"
#include "ndk/guard.h"
#include "ndk/strace.h"

#ifdef NDK_RTLOG
# include <cstdio>
#endif

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
  cobj_factory_(cof)
{
  STRACE("");
  if (cobj_factory_ == 0)
    cobj_factory_ = new lru_cache_object_factory();
  //cache_priority_queue_ = new cache_heap<KEY>(0, max_size);
  cache_priority_queue_ = new cache_deque<KEY>();
}
template<typename KEY, typename SYNCH_MUTEX>
cache_manager<KEY, SYNCH_MUTEX>::~cache_manager()
{
  STRACE("");
  if (this->cache_priority_queue_)
    delete this->cache_priority_queue_;
  this->cache_priority_queue_ = 0;
}
template<typename KEY, typename SYNCH_MUTEX>
inline cache_object *cache_manager<KEY, SYNCH_MUTEX>::get(const KEY &key)
{
  STRACE("");
  guard<SYNCH_MUTEX> g(this->cache_mutex_);
  return this->get_i(key);
}
template<typename KEY, typename SYNCH_MUTEX>
inline void cache_manager<KEY, SYNCH_MUTEX>::release(cache_object *cobj)
{
  STRACE("");
  guard<SYNCH_MUTEX> g(this->cache_mutex_);
  cobj->release();
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_manager<KEY, SYNCH_MUTEX>::put(const KEY &key, 
                                                void *data,
                                                size_t size,
                                                cache_object_observer *ob,
                                                cache_object *&cobj)
{
  STRACE("");
  guard<SYNCH_MUTEX> g(this->cache_mutex_);
  return this->put_i(key, data, size, ob, cobj);
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_manager<KEY, SYNCH_MUTEX>::flush(void)
{
  STRACE("");
  guard<SYNCH_MUTEX> g(this->cache_mutex_);
  return this->flush_i();
}
template<typename KEY, typename SYNCH_MUTEX>
inline cache_object *cache_manager<KEY, SYNCH_MUTEX>::get_i(const KEY &key)
{
  STRACE("");
  cache_map_itor itor = this->cache_map_.find(key);
  if (itor == this->cache_map_.end())
    // try to find <key> in pending list.
    return this->get_pending_obj(key);

  // this method will call cache_object::acquire to update priority.
  this->cache_priority_queue_->adjust(itor->second->proxy());

  return itor->second;
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_manager<KEY, SYNCH_MUTEX>::put_i(const KEY &key, 
                                                  void *data,
                                                  size_t size,
                                                  cache_object_observer *ob,
                                                  cache_object *&cobj)
{
  STRACE("");
  if (data == 0)  return -1;

  // # get an new objects by factory.
  if (this->make_cobj(data, size, ob, cobj) == -1)
    return -1;

  // # insert to map.
  std::pair<cache_map_itor, bool> ret = 
    this->cache_map_.insert(std::make_pair(key, cobj));
  if (!ret.second) // insert failed. the pair was actually inserted
  {
    NDK_LOG("warning: insert failed, the pair was actually inserted! [%s:%d]",
            __FILE__,
            __LINE__);
    this->cobj_factory_->destroy(cobj);
    cobj = 0;
    return -1;
  }

  // # build priority before push to heap.
  cobj->acquire(); // update acquire record before insert to heap.

  // # push to heap.
  if (this->cache_priority_queue_->insert(key, cobj) != 0)
  {
    NDK_LOG("insert to heap failed![%s:%d]", __FILE__, __LINE__);
    this->cache_map_.erase(key);
    this->cobj_factory_->destroy(cobj);
    cobj = 0;
    return -1;
  }

  // # increase current water mark.
  this->water_mark_ += cobj->size();
  return 0;
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_manager<KEY, SYNCH_MUTEX>::drop_i(const KEY &key, 
                                                   cache_object *cobj)
{
  STRACE("");
  if (cobj->refcount() == 0)
  {
    cobj->drop();
    this->cobj_factory_->destroy(cobj);
  }else
  {
    this->pending_list_.insert(std::make_pair(key, cobj));
    return -1;
  }
  return 0;
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_manager<KEY, SYNCH_MUTEX>::make_cobj(void *data, 
                                                      size_t size, 
                                                      cache_object_observer *ob,
                                                      cache_object *&obj)
{
  STRACE("");
  if (size > this->cobj_max_size_
      || size < this->cobj_min_size_) // 
  {
    NDK_LOG("error: [%u] is too small/large to cache", size);
    return -1;
  }

  // # check water mark.
  if (size + this->water_mark_ > this->high_water_mark_)
  {
    // drop some cache-objects.
    do
    {
      if (this->flush_i() == -1)
      {
        //NDK_LOG("warning: flush failed!");
        return -1;
      }
    }while (this->water_mark_ > this->low_water_mark_);
  }

  // # object factory.
  obj = this->cobj_factory_->create(data, size, ob);
  return 0;
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_manager<KEY, SYNCH_MUTEX>::flush_i(void)
{
  STRACE("");

  // 1. drop all of pending objs.
  if (this->flush_pending_objs() == 0) return 0;

  // 2.
  KEY temp_key;
  cache_object *temp_obj = 0;
  if (this->cache_priority_queue_->remove(temp_key, temp_obj) == 0)
  {
    this->cache_map_.erase(temp_key);
    size_t s = temp_obj->size();
    if (this->drop_i(temp_key, temp_obj) == 0)
      this->water_mark_ -= s;
    return 0;
  }
  return -1;
}
template<typename KEY, typename SYNCH_MUTEX>
inline cache_object *cache_manager<KEY, SYNCH_MUTEX>::get_pending_obj(const KEY &key)
{
  STRACE("");
  if (!this->pending_list_.empty())
  {
    cache_map_itor pos = this->pending_list_.find(key);
    if (pos != this->pending_list_.end())
    {
      pos->second->acquire();
      return pos->second;
    }
  }
  return 0;
}
template<typename KEY, typename SYNCH_MUTEX>
int cache_manager<KEY, SYNCH_MUTEX>::flush_pending_objs()
{
  STRACE("");
  if (!this->pending_list_.empty())
  {
#if 0
    NDK_LOG("warning: pending queue size = %d, map queue size = %d, "
            "water mark = %ld",
            this->pending_list_.size(),
            this->cache_map_.size(),
            this->water_mark_);
#endif
    cache_map_itor itor = this->pending_list_.begin();
    for (; itor != this->pending_list_.end();)
    {
      if (itor->second->refcount() == 0)
      {
        this->water_mark_ -= itor->second->size();
        itor->second->drop();
        this->cobj_factory_->destroy(itor->second);
        this->pending_list_.erase(itor++);
      }else
        ++itor;
    }
    if (this->water_mark_ <= this->low_water_mark_)
      return 0;
  }
  return -1;
}
template<typename KEY, typename SYNCH_MUTEX>
int cache_manager<KEY, SYNCH_MUTEX>::flush_pending_objs(const KEY &key)
{
  STRACE("");
  if (!this->pending_list_.empty())
  {
    cache_map_itor pos = this->pending_list_->find(key);
    if (pos != this->pending_list_.end())
    {
      if (pos->second->refcount() == 0)
      {
        this->water_mark_ -= pos->second->size();
        pos->second->drop();
        this->cobj_factory_->destroy(pos->second);
        this->pending_list_.erase(pos);
        return 0;
      }
    }
  }
  return -1;
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_manager<KEY, SYNCH_MUTEX>::flush_i(const KEY &key)
{
  STRACE("");
  cache_map_itor itor = this->cache_map_.find(key);
  if (itor == this->cache_map_.end())
    return this->flush_pending_objs(key);

  cache_object *temp_obj = itor->second;
  this->cache_map_.erase(itor);

  this->cache_priority_queue_->remove(temp_obj->proxy());
  size_t s = temp_obj->size();
  int result = this->drop_i(key, temp_obj);
  if (result == 0)
    this->water_mark_ -= s;
  return result;
}
template<typename KEY, typename SYNCH_MUTEX>
void cache_manager<KEY, SYNCH_MUTEX>::check(void)
{
  this->cache_priority_queue_->check();
}
} // namespace ndk
#endif // NDK_CACHE_MANAGER_CPP_
