// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-20 22:50
 */
//========================================================================

#ifndef NDK_MEM_POOL_H_
#define NDK_MEM_POOL_H_

#include "ndk/thread_mutex.h"

#include <cstdio>
#include <malloc.h>

namespace ndk
{
  /**
   * @class obj_header 
   *
   * @brief This information is stored in memory allocated by the memory pool. 
   */
  /*************************************************************************
   *
   *          +------------> This is a slab, size = 1M, 
   *          |
   * +---------------------+   
   * |                     |
   * |    slab_header:0    |        --> Slabs are chained to double-link.
   * |                     |
   * +---------------------+
   *          |
   *         \|/
   * +---------------------+
   * |   obj_header:inuse  |<---+
   * |------------+--------+    |
   * |            |  prev  |--> 0
   * +------------+--------+    |
   * |            |  next  |--+ |  
   * |=====================+  | |
   * |                     |  | |
   * |                     |  | |
   * |                     |  | |
   * |    Useable Memory   |  | |   --> Application acquire this object, size = 2K.
   * |                     |  | |
   * |                     |  | |
   * |                     |  | |
   * +---------------------+  | |
   * |    obj_header:free  |<-+-++
   * +------------+--------+  | ||
   * |            |  prev  |--+-++
   * +------------+--------+     |
   * |            |  next  |--+  |  
   * |=====================+  |  |
   * |                     |  |  |
   * |                     |  |  |
   * | Useable Memory      |  |  |  --> Application release this object, size = 1.5K.
   * |                     |  |  |
   * |                     |  |  |
   * +---------------------+  |  |
   * |   obj_header:inuse  |<-+-+|
   * +------------+--------+    ||
   * |            |  prev  |----++
   * +------------+--------+    |
   * |            |  next  |--+ | 
   * |=====================+  | |
   * |                     |  | |
   * |                     |  | |
   * |                     |  | |
   * |                     |  | |
   * |                     |  | |
   * |                     |  | |
   * | Useable Memory      |  | |   --> Application acquire this object, size = 3K.
   * |                     |  | |
   * |                     |  | |
   * |                     |  | |
   * |                     |  | |
   * |                     |  | |
   * |                     |  | |
   * +---------------------+  | |
   * |   obj_header:free   |<-+-+   --> Seriate free memory,
   * +------------+--------+    |
   * |            |  prev  |----+
   * +------------+--------+  
   * |            |  next  |--> 0 
   * |=====================+   
   * |                     |   
   * |                     |   
   * |                     |   
   *          ....
   *          ....
   * |                     |   
   * |                     |   
   * +---------------------+  
   *
   */

 union max_align_info
 {
   int(*i)();
   void *p;
   long l;
   double d;
 };

#define MALLOC_ALIGN (sizeof(max_align_info))
#define MALLOC_ROUNDUP(X, Y) (((X) +((Y) - 1)) & (~((Y) - 1))) 

#if defined (DUMP_INFO)
#   undef MALLOC_ALIGN
#endif

 /**
  * @class mem_pool
  *
  * @brief This is a memory pool based slabs technic.
  */
 class mem_pool
 {
 public:
   //
   virtual ~mem_pool();

   //
   mem_pool();

   /**
    *
    * <slab_size>			: The size of slab be alloced one time.
    * 		    				  Note: <slab_size> must greater than size 
    * 		    				  of(obj_header).
    *
    * <prealloc_slab_num> 	: The number of slabs prealloc when mem-pool init.
    *
    * <max_slabs>			: The max size of this memory pool apply from OS.
    * 				    		  If the value is 0 or -1, then the max size is unlimited.
    */
   int init(int slab_size = 1024 * 1024/*1M*/, 
            int prealloc_slab_num = 128,
            int max_slabs = 1024/*1G*/);

   void *malloc(unsigned int size);

   void *alloc(unsigned int size);

   void free(void *);

   void dump(void);
 protected:
   /**
    * @class obj_header 
    *
    * @brief This object be stored in every memory object that is alloced 
    * by application, store the slab id , object size and so on.
    */
   class slab_header;
   class obj_header
   {
   public:
     obj_header();
   public:
     int inuse_;

     // Size of the memory pass to application(not include 
     // sizeof(obj_header) 
     unsigned int size_;

     // The slab which this memory object places.
     slab_header *slab_;

     // Prev object.
     obj_header *prev_;

     // Next object.
     obj_header *next_;

#if defined(MALLOC_ALIGN)
     // For align
     char align[MALLOC_ROUNDUP(sizeof(int) + sizeof(unsigned int) + \
                               sizeof(slab_header *) + sizeof(obj_header *) * 2, \
                               MALLOC_ALIGN) \
       - (sizeof(int) + sizeof(unsigned int) + sizeof(slab_header *) \
          + sizeof(obj_header *) * 2)];
#endif
   };
   /**
    * @class slab_header
    *
    * @brief This object be stored in every slab. store the memory header
    * pointer, size ,the scale of being used and so on.
    */
   class slab_header
   {
   public:
     slab_header(unsigned int slab_size);
   public:
     // Num of objects active in slab
     unsigned int inuse_objs_;

#ifdef DUMP_INFO
     // The size of free memory actually in this slab(maybe not seriate).
     unsigned int usable_size_;
#endif
     // Max size of seriate object in this slab.
     unsigned int max_seriate_size_;

     obj_header *head_;

     obj_header *max_seriate_free_head_;

     // =
     slab_header *prev_;

     slab_header *next_;
   };

   void init_object(void *, slab_header *slab, unsigned int size);

   slab_header *alloc_slabs(unsigned int count);

   slab_header *find_usable_slab(unsigned int size);

   void ajust_slab_order(slab_header *slab, unsigned int alloc_size);

   obj_header *pop_usable_object(slab_header *slab, unsigned int alloc_size);
 private:
   // = Config
   unsigned int slab_size_;

   unsigned int prealloc_slab_num_;

   unsigned int max_slabs_;

   // Current alloced slabs
   unsigned int alloced_slabs_;

   // = List of slabs.
   slab_header *slabs_head_;

   slab_header *slabs_tail_;

   // Slabs mutex.
   thread_mutex mutex_;
 };
} // namespace ndk

#endif // NDK_MEM_POOL_H_

