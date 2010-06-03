// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-02-27 00:25
 */
//========================================================================

#ifndef NDK_CACHE_MANAGER_H_
#define NDK_CACHE_MANAGER_H_

#include <map>

#include "ndk/cache_heap.h"
#include "ndk/cache_deque.h"
#include "ndk/cache_object.h"
#include "ndk/global_macros.h"

namespace ndk
{
  class cache_object_factory;
  /**
   * @class cache_manager
   * 
   * @brief
   */
  template<typename KEY, typename SYNCH_MUTEX>
  class cache_manager
  {
  public:
    cache_manager(int min_obj_size = 1024,        // min size of cache object.
                  int max_obj_size = 20*1024*1024,// max size of cache object. 
                  int high_water_mark = 1024,     // max size of cache in MB
                  int low_water_mark  = 960,      // min size of cache in MB
                                                  // remaind when expiring after 
                                                  // high water mark  has been reached
                  cache_object_factory *cof = 0);

    virtual ~cache_manager();

    // Retrieve the object associated with key from cache. 
    // Return 0 on failure, other on success. 
    cache_object *get(const KEY &key);

    //
    void release(cache_object *cobj);

    // Inserts or replaces object associated with key into cache.
    // Return 0 on success, -1 on failure.
    // Key would be removd from manager if data is NULL.
    int put(const KEY &key, void *data, 
            size_t size,
            cache_object_observer *ob,
            cache_object *&cobj);

    // Removes lowest priority object from cache.
    int flush(void);

    // Removes all objects from cache.
    int flush_all(void);

    // Debug.
    void check();
    void dump();
  protected:
    //
    cache_object *get_i(const KEY &key);

    //
    int put_i(const KEY &key, void *data,
              size_t size,
              cache_object_observer *ob,
              cache_object *&cobj);
    //
    int put_ii(const KEY &key, cache_object *&cobj);

    //
    int make_cobj(void *data, size_t size, 
                  cache_object_observer *ob,
                  cache_object *&obj);

    //
    int drop_i(const KEY &key, cache_object *cobj);

    //
    int flush_i(void); 

    int flush_i(const KEY &key); 

    // pending objects opertions.
    //
    int flush_pending_objs(const KEY &key);

    //
    int flush_pending_objs();

    //
    cache_object *get_pending_obj(const KEY &key);
  protected:
    //
    typedef std::map<KEY, cache_object *> cache_map_t;
    typedef typename std::map<KEY, cache_object *>::iterator cache_map_itor;

  protected:
    size_t cobj_min_size_;

    size_t cobj_max_size_;

    unsigned long water_mark_;

    unsigned long high_water_mark_;

    unsigned long low_water_mark_;

    SYNCH_MUTEX cache_mutex_;

    // store hot cache objects.
    cache_map_t cache_map_;  // For fast querying

    cache_deque<KEY> *cache_priority_queue_;

    // store pending objects that refcount > 0
    cache_map_t pending_list_;

    cache_object_factory *cobj_factory_;
  };
} // namespace ndk

#include "ndk/cache_manager.cpp"
#endif // NDK_CACHE_MANAGER_H_

