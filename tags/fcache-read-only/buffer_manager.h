// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-05-24 22:01
 */
//========================================================================

#ifndef BUFFER_MANAGER_H_
#define BUFFER_MANAGER_H_

#include <ndk/singleton.h>
#include <ndk/thread_mutex.h>
#include <cassert>

/**
 * @class buffer_manager
 * 
 * @brief
 */
class buffer_manager
{
public:
  class buffer_item
  {
  public:
    buffer_item()
      : buffer_(0),
      next_(0)
    { }
    char *buffer_;
    buffer_item *next_;
  };
public:
  buffer_manager(size_t block_size)
  : block_size_(block_size),
    n_alloced_(0),
    buffer_list_(0),
    free_buffer_list_(0)
  {
  }
  char *malloc()
  {
    ndk::guard<ndk::thread_mutex> g(this->mutex_);
    ++this->n_alloced_;
    if (this->buffer_list_ == 0)
    {
      this->buffer_list_ = this->alloc_item();
      this->buffer_list_->buffer_ = new char[this->block_size()];
    }

    char *p = this->buffer_list_->buffer_;
    buffer_item *item = this->buffer_list_;
    this->buffer_list_ = this->buffer_list_->next_;
    item->next_ = 0;
    this->free_item(item);
    return p;
  }

  void free(char *p)
  {
    ndk::guard<ndk::thread_mutex> g(this->mutex_);
    --this->n_alloced_;
    assert(p != 0);
    buffer_item *item = this->alloc_item();
    item->buffer_ = p;
    item->next_ = this->buffer_list_;
    this->buffer_list_ = item;
  }

  void flush(size_t blocks = 0)
  {
    ndk::guard<ndk::thread_mutex> g(this->mutex_);
    buffer_item *item = this->buffer_list_;
    size_t count = 0;
    while (item != 0)
    {
      buffer_item *p = item;
      item = item->next_;

      delete []p->buffer_;
      this->free_item(p);

      ++count;
      if (blocks > 0 && count == blocks) 
        break;
    }
    this->buffer_list_ = item;
  }

  inline size_t alloc_blocks()
  { return this->n_alloced_; }

  inline size_t block_size()
  { return this->block_size_; }
protected:
  buffer_item *alloc_item()
  {
    if (this->free_buffer_list_ == 0)
    {
      buffer_item *itor = 0;
      for (int i = 0; i < 64; ++i)
      {
        if (this->free_buffer_list_ == 0)
        {
          this->free_buffer_list_ = new buffer_item();
          itor = this->free_buffer_list_;
        }else
        {
          itor->next_ = new buffer_item();
          itor = itor->next_;
        }
      }
    }
    buffer_item *item = this->free_buffer_list_;
    this->free_buffer_list_ = this->free_buffer_list_->next_;
    item->next_ = 0;
    return item;
  }
  void free_item(buffer_item *item)
  {
    item->next_ = this->free_buffer_list_;
    this->free_buffer_list_ = item;
  }
private:
  size_t block_size_;
  size_t n_alloced_;
  buffer_item *buffer_list_;
  buffer_item *free_buffer_list_;
  ndk::thread_mutex mutex_;
};

#endif // BUFFER_MANAGER_H_

