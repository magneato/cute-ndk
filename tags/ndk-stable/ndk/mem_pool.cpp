#include "ndk/mem_pool.h"

namespace ndk
{
#define PREALLOC_SLAB_NUM   64

#define INUSE_IDENTIRY    	0x13

#define ORDER_BY_AHEAD      0x110
#define ORDER_BY_BACK		    0x112

mem_pool::obj_header::obj_header()
  : inuse_(0),
  size_(0),
  slab_(0),
  prev_(0),
  next_(0)
{
}
mem_pool::slab_header::slab_header(unsigned int slab_size)
  : inuse_objs_(0),
#ifdef DUMP_INFO
  usable_size_(slab_size),
#endif
  max_seriate_size_(slab_size),
  head_(0),
  max_seriate_free_head_(0),
  prev_(0),
  next_(0)
{
}
mem_pool::mem_pool()
: slab_size_(0),
  prealloc_slab_num_(0),
  max_slabs_(0),
  alloced_slabs_(0),
  slabs_head_(0),
  slabs_tail_(0)
{
}
mem_pool::~mem_pool()
{
}
int mem_pool::init(int slab_size/* = 1024 * 1024*/,
                   int prealloc_slab_num/* = 128*/,
                   int max_slabs/* = 1024*/)
{
  // 1. Assign value.
  this->slab_size_ = slab_size;// + sizeof(obj_header);
  if (max_slabs <= 0)
  {
    fprintf(stderr, "max slabs is too small [%d]\n", max_slabs);
    return -1;
  }
  this->max_slabs_ = max_slabs;
  this->prealloc_slab_num_ = prealloc_slab_num;

  // 2. Check value. 
  if (this->slab_size_ <= sizeof(obj_header))
  {
    fprintf(stderr, "slab size is too small [%d]\n", slab_size);
    return -1;
  }
#if defined(MALLOC_ALIGN)
  this->slab_size_ = MALLOC_ROUNDUP(this->slab_size_, MALLOC_ALIGN);
#endif

  // 3. Prealloc memory.
  if (this->prealloc_slab_num_ <= 0)
    this->prealloc_slab_num_ = PREALLOC_SLAB_NUM;

  this->alloc_slabs(this->prealloc_slab_num_);

  return 0;
}
mem_pool::slab_header *mem_pool::alloc_slabs(unsigned int count)
{
  if (this->alloced_slabs_ >= this->max_slabs_)
    return 0;
  slab_header *ret_val = 0;
  for (unsigned int i = 0; i < count; ++i)
  {
    void *memory = ::new char[this->slab_size_ + sizeof(obj_header)];
    if (memory)
    {
      //
      slab_header *slab = new slab_header(this->slab_size_);
      if (slab)
      {
        if (ret_val == 0)
          ret_val = slab;
        // 1. Init memory header.
        this->init_object(memory, 
                          slab, 			  // Loader.
                          this->slab_size_  // Actual usable memory size.
                         );

        // 2. Init slab header.
        slab->head_ = reinterpret_cast<obj_header *>(memory);
        slab->max_seriate_free_head_ = slab->head_;

        // 3. Linking.
        if (this->slabs_head_ == 0)  // The first slab.
        {
          this->slabs_head_ = slab;
          this->slabs_tail_ = slab;
        }else  // Push back
        {
          this->slabs_tail_->next_ = slab;
          slab->prev_ = this->slabs_tail_;
          this->slabs_tail_ = slab;
        }

        // 4. Counter.
        ++this->alloced_slabs_;
        if (this->alloced_slabs_ >= this->max_slabs_)
          break;
        continue;
      }
      // 5. Exception, goto Error.
      ::delete []((char *)memory);
    }
    // Error.
    fprintf(stderr, "Prealloc memory slabs failed, "
            "slab size = %d, index = %d\n", 
            this->slab_size_ + static_cast<unsigned int>(sizeof(obj_header)), 
            i);
    break;
  }
  return ret_val;
}
void mem_pool::init_object(void *memory, slab_header *slab, unsigned int size)
{
  obj_header *oh = reinterpret_cast<obj_header *>(memory);
  oh->size_ = size;
  oh->slab_ = slab;
  oh->prev_ = 0;
  oh->next_ = 0;
}
void *mem_pool::malloc(unsigned int size)
{
  //return ::malloc(size);
  if (size > this->slab_size_) return 0;
  // Lock. 
  this->mutex_.acquire();
#if defined(MALLOC_ALIGN)
  size = MALLOC_ROUNDUP(size, MALLOC_ALIGN);
#endif
  // 1. Find the first usable slab in the mempool.
  slab_header *usable_slab = this->find_usable_slab(size);
  if (usable_slab == 0)
    usable_slab = this->alloc_slabs(this->prealloc_slab_num_);

  if (usable_slab == 0) { this->mutex_.release(); return 0; }

  // 2. Find the usable seriate chunk' obj_header in the slab. 
  obj_header *usable_object = this->pop_usable_object(usable_slab, 
                                                      size);

  // 3. Adjust slab's order. 
  this->ajust_slab_order(usable_slab, ORDER_BY_AHEAD);

#ifdef DUMP_INFO
  // For dump info
  usable_slab->usable_size_ -= usable_object->size_;
  if (usable_slab->max_seriate_size_ != this->slab_size_)
    usable_slab->usable_size_ -= sizeof(obj_header);
#endif
  this->mutex_.release();
  return reinterpret_cast<void *>(usable_object + 1);
}
mem_pool::slab_header *mem_pool::find_usable_slab(unsigned int size)
{
  slab_header *slab = this->slabs_head_;
  while (slab)
  {
    if (slab->max_seriate_size_ >= size)
      return slab;
    else
      slab = slab->next_;
  }
  return 0;
}
void mem_pool::ajust_slab_order(slab_header *slab, unsigned int order_type)
{
  if (order_type == ORDER_BY_AHEAD)
  {
    if (slab == this->slabs_head_)
      return ;
    while (slab->prev_)  // roll ahead
    {
      if (slab->prev_->max_seriate_size_ > slab->max_seriate_size_)
      {
        // change order.
        slab_header *prev_slab = slab->prev_;
        // 1. 
        if (prev_slab->prev_) // not header
          prev_slab->prev_->next_ = slab;
        // 2. 
        prev_slab->next_ = slab->next_;
        // 3. 
        if (slab->next_) // not tail
          slab->next_->prev_ = prev_slab;
        // 4.
        slab->prev_ = prev_slab->prev_;
        // 5. 
        prev_slab->prev_ = slab;
        // 6.
        slab->next_ = prev_slab;

        // 7. Reset head or tail 
        if (prev_slab->next_ == 0)
          this->slabs_tail_ = prev_slab;

        if (slab->prev_ == 0)
          this->slabs_head_ = slab;
      }else
        break;
    }
  }else if (order_type == ORDER_BY_BACK)
  {
    if (slab == this->slabs_tail_)
      return ;
    while (slab->next_) // roll back.
    {
      if (slab->next_->max_seriate_size_ < slab->max_seriate_size_)
      {
        // change order.
        slab_header *next_slab = slab->next_;
        // 1.
        if (next_slab->next_) // // not tail
          next_slab->next_->prev_ = slab;
        // 2. 
        slab->next_ = next_slab->next_;
        // 3.
        if (slab->prev_)
          slab->prev_->next_ = next_slab;
        // 4.
        next_slab->prev_ = slab->prev_;
        // 5.
        next_slab->next_ = slab;
        // 6.
        slab->prev_ = next_slab;

        // 7. Reset head or tail 
        if (slab->next_ == 0)
          this->slabs_tail_ = slab;

        if (next_slab->prev_ == 0)
          this->slabs_head_ = next_slab;
      }else
        break;
    }
  }	
  return ;
}
mem_pool::obj_header *mem_pool::pop_usable_object(slab_header *slab, 
                                                  unsigned int alloc_size)
{
  slab->max_seriate_size_ -= alloc_size;
  obj_header *free_obj = slab->max_seriate_free_head_;
  // Set this new object
  free_obj->slab_  = slab;
  free_obj->inuse_ = INUSE_IDENTIRY;  // !!!
  if (slab->max_seriate_size_ > 0 
      && slab->max_seriate_size_ > sizeof(obj_header) // 
     )
  {
    obj_header *next_object = 
      reinterpret_cast<obj_header *>((char*)free_obj 
                                     + sizeof(obj_header) // 
                                     + alloc_size  //
                                    );
    if (free_obj->next_)
      free_obj->next_->prev_ = next_object;

    free_obj->size_  = alloc_size;
    next_object->next_ = free_obj->next_;

    next_object->slab_ = slab;

    free_obj->next_ = next_object;
    next_object->prev_ = free_obj;

    // skip next new chunk's object header
    slab->max_seriate_size_ -= sizeof(obj_header);
    next_object->size_ = slab->max_seriate_size_;

    slab->max_seriate_free_head_ = next_object;
  }else
  {
    // The remainder memory that is less than sizeof(obj_header), 
    // then pass it to application.
    slab->max_seriate_free_head_ = 0;
    slab->max_seriate_size_ = 0;
  }

  ++slab->inuse_objs_;
  return free_obj; 
}
void mem_pool::free(void *memory)
{
  //if (memory) ::free(memory); return ;
  // Lock. 
  this->mutex_.acquire();
  // Check
  if (memory == 0 || this->slabs_head_ == 0) 
  { 
    fprintf(stderr, "memory = %p or slabs_head_ = %p\n",
            memory, 
            this->slabs_head_);
    this->mutex_.release(); 
    return ; 
  }

  obj_header *free_obj =((obj_header*)memory) - 1;

  // This memory maybe did not create in this pool.
  if (free_obj->inuse_ != INUSE_IDENTIRY)
  { 
    fprintf(stderr ,"this memory is not in pool\n");
    this->mutex_.release(); 
    return ; 
  }
#ifdef DUMP_INFO
  // For dump info 
  free_obj->slab_->usable_size_ += free_obj->size_ + sizeof(obj_header);
#endif

  free_obj->inuse_ = 0;
  // 1. 
  if (free_obj->prev_ != 0 && free_obj->next_ != 0)
  {
    // Merge prev chunk
    if (!free_obj->prev_->inuse_) // prev chunk is free.
    {
#ifdef DUMP_INFO_1
      // For dump info 
      free_obj->slab_->usable_size_ += sizeof(obj_header);
#endif

      // merge this new return chunk.
      free_obj->prev_->next_  = free_obj->next_;
      free_obj->next_->prev_  = free_obj->prev_;
      free_obj->prev_->size_ += sizeof(obj_header) + free_obj->size_;
      free_obj = free_obj->prev_;  // !!!
    }
    // Merge next chunk
    if (!free_obj->next_->inuse_) // next chunk is free.
    {
#ifdef DUMP_INFO_1
      // For dump info 
      free_obj->slab_->usable_size_ += sizeof(obj_header);
#endif
      if (free_obj->next_->next_)
        free_obj->next_->next_->prev_ = free_obj;

      free_obj->size_ += sizeof(obj_header) + free_obj->next_->size_;
      free_obj->next_ = free_obj->next_->next_;
    }
  }else if (free_obj->prev_ != 0) // tail chunck
  {
    if (!free_obj->prev_->inuse_) // prev chunk is free.
    {
#ifdef DUMP_INFO_1
      // For dump info 
      free_obj->slab_->usable_size_ += sizeof(obj_header);
#endif

      // merge this new return chunk.
      free_obj->prev_->next_  = 0; // free_obj->next_;
      free_obj->prev_->size_ += sizeof(obj_header) + free_obj->size_;
      free_obj = free_obj->prev_;  // !!!
    }
  }else if (free_obj->next_ != 0) // head chunk
  {
    if (!free_obj->next_->inuse_) // next chunk is free. 
    {
#ifdef DUMP_INFO_1
      // For dump info 
      free_obj->slab_->usable_size_ += sizeof(obj_header);
#endif

      // merge this new return chunk.
      free_obj->size_ += sizeof(obj_header) + free_obj->next_->size_;
      if (free_obj->next_->next_)
        free_obj->next_->next_->prev_ = free_obj;
      free_obj->next_ = free_obj->next_->next_;
    }
  }else // free_obj->next_ == 0 && free_obj->prev_ == 0
  {
    fprintf(stderr, "error memory block!\n");
  }

  // 2. Ajust slab.
  if (free_obj->size_ > free_obj->slab_->max_seriate_size_)
  {
    free_obj->slab_->max_seriate_size_ = free_obj->size_;
    free_obj->slab_->max_seriate_free_head_ = free_obj;
  }
  this->ajust_slab_order(free_obj->slab_, ORDER_BY_BACK);

  --(free_obj->slab_->inuse_objs_);
  this->mutex_.release(); 
  return ;
}
void mem_pool::dump(void)
{
#ifdef DUMP_INFO
  this->mutex_.acquire();
  slab_header *slab = this->slabs_head_;
  fprintf(stderr,
          "\t"
          "%-12s"
          "%-8s"
          "%-12s"
          "%-8s"
          "%-12s"
          "%-12s"
          "%-12s\n"
          , "addr"
          , "slab"
          , "size"
          , "inuse"
          , "used"
          , "free"
          , "max-seriate");
  int index = 0;
  while (slab)
  {
    fprintf(stderr,
            "\t"
            "%-12p"
            "%-8d"
            "%-12d"
            "%-8d"
            "%-12d"
            "%-12d"
            "%-12d\n"
            , slab
            , index++
            , this->slab_size_
            , slab->inuse_objs_
            , this->slab_size_ - slab->usable_size_
            , slab->usable_size_
            , slab->max_seriate_size_);
    slab = slab->next_;
  }
  this->mutex_.release(); 
#endif
}
} // namespace ndk

