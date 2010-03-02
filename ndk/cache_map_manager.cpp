#ifndef NDK_CACHE_MAP_MANAGER_CPP_
#define NDK_CACHE_MAP_MANAGER_CPP_

#include "ndk/cache_map_manager.h"

namespace ndk
{
template<typename KEY, typename SYNCH_MUTEX>
cache_map_manager::cache_map_manager(caching_strategy *cs,
                                     cache_object_factory *cof)
{
}
template<typename KEY, typename SYNCH_MUTEX>
cache_map_manager::~cache_map_manager()
{
}
template<typename KEY, typename SYNCH_MUTEX>
inline cache_object * cache_map_manager::get(const KEY &key)
{
  guard<SYNCH_MUTEX> g(this->cache_mutex_);
  return this->get_i(key, cobj);
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_map_manager::get_i(const KEY &key, cache_object *&cobj)
{
  cache_map_itor itor = this->cache_map_.find(key);
  if (itor == this->cache_map_.end())
    return 0;
  return itor->second;
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_map_manager::put(const KEY &key, 
                                  cache_object *cobj)
{
  guard<SYNCH_MUTEX> g(this->cache_mutex_);
  return this->put_i(key, cobj);
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_map_manager::put_i(const KEY &key, 
                                    const void *data, 
                                    size_t size,
                                    cache_object *&cobj)
{
  if (data == 0) 
  {
    this->flush_i_(key);
    cobj = 0;
    return 0;
  }
  if (this->make_cobj(data, size, cobj) == -1)
    return -1;
  
  std::pair<cache_map_itor, bool> ret = 
    this->cache_map_.insert(std::make_pair(key, cobj));
  if (!ret.second) // insert failed. the pair was actually inserted
  {
    fprintf(stderr, 
            "warning: insert failed, the pair was actually inserted! [%s:%d]\n",
            __FILE__,
            __LINE__);
    cache_object *old_obj = ret.first->second;
    this->drop_i(old_obj);

    this->cache_map_.erase(ret.first);
    this->cache_map_.insert(std::make_pair(key, cobj));
  }
  cobj->acquire();
  return 0;
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_map_manager::drop(cache_object *&cobj)
{
  guard<SYNCH_MUTEX> g(this->cache_mutex_);
  return this->drop_i(cobj);
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_map_manager::drop_i(cache_object *&cobj)
{
  return 0;
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_map_manager::make_cobj(const void *data, 
                                        size_t size, 
                                        cache_object *&obj)
{
  if (size > this->cobj_max_size_
      || size < this->cobj_min_size_) // 
  {
    fprintf(stderr, 
            "error: [%u] is too small/large to cache\n", 
            size);
    return -1;
  }
  if (size + this->water_mark_ > this->high_water_mark_)
  {
    // drop some object.
    do
    {
      if (this->flush_i() == -1)
      {
        fprintf(stderr, 
                "error: flush error!\n");
        return -1;
      }
    }while (this->water_mark_ > this->low_water_makr_);
  }
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_map_manager::flush()
{
  guard<SYNCH_MUTEX> g(this->cache_mutex_);
  return this->flush_i();
}
template<typename KEY, typename SYNCH_MUTEX>
inline int cache_map_manager::flush_i()
{
  return 0;
}
} // namespace ndk
#endif // NDK_CACHE_MAP_MANAGER_CPP_
