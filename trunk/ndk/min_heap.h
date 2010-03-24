// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-03-24 12:00
 */
//========================================================================

#ifndef NDK_MIN_HEAP_H_
#define NDK_MIN_HEAP_H_

#include <cassert>

namespace ndk
{
  /**
   * @class min_heap
   * 
   * @brief
   */
  template<typename ITEM>
  class min_heap
  {
  public:
    min_heap(int max_size)
    : max_size_(max_size),
    size_(0),
    heap_(0)
    {
      heap_ = new ITEM *[max_size_];
      for (int i = 0; i < max_size_; ++i) heap_[i] = 0;
    }

    ~min_heap()
    {
      if (this->heap_)
        delete []this->heap_;
      this->heap_ = 0;
    }

    inline bool is_empty() const
    { return this->size_ == 0; }

    inline bool is_full() const
    { return this->size_ == this->max_size_; }

    // return  0 on push successfully.
    // return -1 on failed.
    inline int push(ITEM *item)
    {
      if (this->size_ >= this->max_size_)
        return -1;
      this->shift_up(this->size_++, item);
      return 0;
    }

    // return the pointer of the first of ITEM in heap.
    // return 0 on failed.
    inline ITEM *pop()
    {
      if (this->size_ == 0) return 0;
      ITEM *item = this->heap_[0];
      this->shift_down(0, this->heap_[--this->size_]);
      return item;
    }

    // return the pointer of the ITEM which heap_idx is <idx> in heap.
    // return 0 on failed.
    inline ITEM *pop(int idx)
    {
      if (this->size_ == 0 || idx < 0) return 0;
      ITEM *item = this->heap_[idx];
      assert(this->heap_[idx]->heap_idx_ != -1 &&
             idx == item->heap_idx_);
      --this->size_;
      int parent = (idx - 1) / 2;
      if (*(this->heap_[parent]) > *(this->heap_[this->size_]))
        this->shift_up(idx, this->heap_[this->size_]);
      else
        this->shift_down(idx, this->heap_[this->size_]);
      return item;
    }

    // return  0 on erase <item> ok.
    // return -1 on failed.
    inline int erase(ITEM *item)
    {
      if (item == 0) return -1;
      return this->pop(item->heap_idx_) == 0 ? -1 : 0;
    }

    // adjust ths position of item.
    // you must call this method if you reset the priority of the <item>.
    int adjust(ITEM *item)
    {
      if (item == 0) return -1;

      assert(this->heap_[item->heap_idx_] == item);
      if (this->heap_[item->heap_idx_] != item)
        return -1;
      this->pop(item->heap_idx_);
      //
      return this->push(item);
    }

    // for debug
    void check_heap()
    {
      for (int i = 0, parent = 0; 
           i < this->size_; 
           ++i, parent = (i - 1) / 2)
      {
#if 1
        assert(*(this->heap_[i]) >= *(this->heap_[parent]));
#else
        if (*(this->heap_[i]) < *(this->heap_[parent]))
        {
          fprintf(stderr, "cache heap elements: ");
          for (int i = 0; i < this->size_; ++i)
          {
            fprintf(stderr, "i=%d\tpri=%d\t%p\n", 
                    i, 
                    this->heap_[i]->priority(),
                    this->heap_[i]);
          }
          fprintf(stderr, "\n");
          fprintf(stderr, "cache heap sorted elements: ");
          ITEM *item = 0;
          int i = 0;
          while (1)
          {
            item = this->pop(0);
            if (item == 0) break;
            fprintf(stderr, "i=%d\tpri=%d\t%p\n", 
                    i++, 
                    item->priority(),
                    item);
          }
          fprintf(stderr, "\n");
          assert(0);
        }
#endif
      }
    }
  protected:
    void shift_up(int pos, ITEM *moved_item)
    {
      int parent = (pos - 1) / 2;
      while (pos > 0 && *(this->heap_[parent]) > *moved_item)
      {
        (this->heap_[pos] = this->heap_[parent])->heap_idx_ = pos;
        pos = parent;
        parent = (parent - 1) / 2;
      }
      (this->heap_[pos] = moved_item)->heap_idx_ = pos;
    }
    void shift_down(int pos, ITEM *moved_item)
    {
      int lchild = (pos * 2) + 1;
      while (lchild < this->size_)
      {
        if ((lchild + 1) < this->size_ 
            && *(this->heap_[lchild]) > *(this->heap_[lchild + 1]))
          ++lchild;
        if (*moved_item <= *(this->heap_[lchild]))
          break;
        else
        {
          (this->heap_[pos] = this->heap_[lchild])->heap_idx_ = pos;
          pos = lchild;
          lchild = (2 * lchild) + 1;
        }
      }
      (this->heap_[pos] = moved_item)->heap_idx_ = pos;
    }
  private:
    // Max capbility of heap.
    int max_size_;

    // Current size of heap.
    int size_;

    // Heap struct.
    ITEM **heap_;
  };
} // namespace ndk

#endif // NDK_MIN_HEAP_H_

