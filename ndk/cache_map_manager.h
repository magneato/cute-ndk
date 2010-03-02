// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-02-27 00:25
 */
//========================================================================

#ifndef NDK_CACHE_MAP_MANAGER_H_
#define NDK_CACHE_MAP_MANAGER_H_

namespace ndk
{
  /**
   * @class cache_map_manager
   * 
   * @brief
   */
  template<typename KEY, typename SYNCH_MUTEX>
  class cache_map_manager
  {
  public:
    cache_map_manager(caching_strategy *cs, 
                      cache_object_factory *cof);

    virtual ~cache_map_manager();

    // Retrieve the object associated with key from cache. 
    // Return 0 on failure, other on success. 
    cache_object *get(const KEY &key);

    // Inserts or replaces object associated with key into cache.
    // Return 0 on success, -1 on failure.
    int put(const KEY &key, const void *data, size_t size,
            cache_object *&cobj);

    // Decrement reference count on cached object, perhaps delete. 
    // Returns 0 if only decremented, 1 if deleted, -1 if error.
    int drop(cache_object *&cobj);

    // Removes lowest priority object from cache.
    int flush(void);
  protected:
    //
    cache_object *get_i(const KEY &key);

    //
    int put_i(const KEY &key, const cache_object *cobj);

    //
    int drop_i(cache_object *&cobj);

    //
    int flush_i(void); 
  private:
    typedef std::map<KEY, cache_object *> cache_map_t;
    typedef std::map<KEY, cache_object *>::iterator cache_map_itor;

    cache_map_t cache_map_;  // For fast querying

    cache_heap *cache_heap_;  // For fast getting cobj need to drop.

    caching_strategy *caching_strategy_;
  };
} // namespace ndk

#endif // NDK_CACHE_MAP_MANAGER_H_

