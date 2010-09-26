#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <cassert>

class heap_item
{
public:
  heap_item(int val)
  : value_(val),
  index_(-1)
  { }

  int value_;
  int index_;
};
class heap
{
public:
  heap()
  : heap_(0),
    max_size_(1024),
    size_(0)
  {
    heap_ = new heap_item *[max_size_];
    for (int i = 0; i < max_size_; ++i)
      heap_[i] = 0;
  }
  ~heap()
  {
    if (this->heap_)
      delete []heap_;
    this->heap_ = 0;
  }
  int push(int val)
  {
    heap_item *hi = new heap_item(val);
    if (this->size_ >= this->max_size_)
      return -1;
    //this->heap_[this->size_] = hi;
    this->shift_up(this->size_, hi);
    ++this->size_;
    return 0;
  }
  void shift_up(int pos, heap_item *moved_item)
  {
    int parent = (pos - 1) / 2;
    while (pos > 0)
    {
      if (this->heap_[parent]->value_ <= moved_item->value_) 
        break;
      else
      {
        this->heap_[pos] = this->heap_[parent];
        this->heap_[pos]->index_ = pos;
        pos = parent;
        parent = (parent - 1) / 2;
      }
    }
    this->heap_[pos] = moved_item;
    this->heap_[pos]->index_ = pos;
  }
  void shift_down(int pos, heap_item *moved_item)
  {
    int lchild = (pos * 2) + 1;
    while (lchild < this->size_)
    {
      if ((lchild + 1) < this->size_ 
          && this->heap_[lchild]->value_ > this->heap_[lchild + 1]->value_)
        ++lchild;
      if (moved_item->value_ <= this->heap_[lchild]->value_)
        break;
      else
      {
        this->heap_[pos] = this->heap_[lchild];
        this->heap_[pos]->index_ = pos;
        pos = lchild;
        lchild = (2 * lchild) + 1;
      }
    }
    this->heap_[pos] = moved_item;
    this->heap_[pos]->index_ = pos;
    //this->shift_up(pos, moved_item);
  }
  heap_item *pop()
  {
    heap_item *item = this->heap_[0];
    --this->size_;
    this->shift_down(0, this->heap_[this->size_]);
    item->index_ = -1;
    return item;
  }
  heap_item *pop(int pos)
  {
    if (pos < 0 || pos >= this->size_) return 0;
    heap_item *item = this->heap_[pos];
    if (pos != item->index_)
    {
      printf(" pos = %d item->index_ = %d\n", 
             pos,
             item->index_);
    }
    assert(pos == item->index_);
    --this->size_;
    //this->heap_[pos] = this->heap_[this->size_];
    int parent = (pos - 1) / 2;
    if (pos > 0 && this->heap_[parent]->value_ > this->heap_[this->size_]->value_)
      this->shift_up(pos, this->heap_[this->size_]);
    else
      this->shift_down(pos, this->heap_[this->size_]);
    item->index_ = -1;
    return item;
  }
  void dump()
  {
    printf("heap data : ");
    for (int i = 0; i < this->size_; ++i)
    {
      assert(this->heap_[i]->index_ == i);
      //printf("idx=%d i=%d v=%d ~ ", this->heap_[i]->index_, i, this->heap_[i]->value_);
      printf("%d ", this->heap_[i]->value_);
    }
    printf("\n\n");
  }
  void check_heap()
  {
    for (int i = 0; i < this->size_; ++i)
    {
      int parent = (i - 1) / 2;
      if (this->heap_[i]->value_ < this->heap_[parent]->value_)
      {
        printf("error: child %d < parent %d\n",
               this->heap_[i]->value_,
               this->heap_[parent]->value_);
      }
      //assert(this->heap_[i]->value_ >= this->heap_[parent]->value_);
    }
  }
private:
  heap_item **heap_;
  int size_;
  int max_size_;
};
int main ()
{
  srand(time(0));

  heap h;
  for (int i = 0; i < 200; ++i)
  {
    h.push(rand() % 200);
  }

  h.dump();
#if 1
  printf("prepop someone : ");
  for (int i = 0; i < 100; ++i)
  {
    int pos = rand() % 100;
    printf("%d-%d ", pos, h.pop(pos)->value_);//h.pop(rand() % 100));
  }
  printf("\n\n");
  h.dump();

  for (int i = 0; i < 100; ++i)
  {
    h.push(rand() % 200);
  }
  h.dump();
#endif

  h.check_heap();
  // print
  printf("pop heap : ");
  for (int i = 0; i < 200; ++i)
  {
    heap_item *item = h.pop(0);
    printf("%d ", item->value_);
  }
  printf("\n\n");
}
