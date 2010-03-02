#include <stdio.h>
#include <stdlib.h>
#include <time.h>

class heap
{
public:
  heap()
  : heap_(0),
    max_size_(1024),
    size_(0)
  {
    heap_ = new int[max_size_];
    for (int i = 0; i < max_size_; ++i)
      heap_[i] = 0;
  }
  ~heap()
  {
    delete []heap_;
  }
  int push(int val)
  {
    if (this->size_ >= this->max_size_)
      return -1;
    this->heap_[size_] = val;
    this->shift_down(this->size_);
    ++this->size_;
    return 0;
  }
  void shift_down(int pos)
  {
    int val = this->heap_[pos];

    int parent = (pos - 1) / 2;
    while (pos > 0)
    {
      if (this->heap_[parent] <= val) 
        break;
      else
      {
        this->heap_[pos] = this->heap_[parent];
        pos = parent;
        parent = (parent - 1) / 2;
      }
    }
    this->heap_[pos] = val;
  }
  int pop()
  {
    int val = this->heap_[0];
    --this->size_;
    this->heap_[0] = this->heap_[this->size_];
    this->shift_up(0);
    return val;
  }
  int pop(int pos)
  {
    if (pos < 0 || pos >= this->size_) return -1;
    int val = this->heap_[pos];
    --this->size_;
    this->heap_[pos] = this->heap_[this->size_];
    int parent = (pos - 1) / 2;
    if (this->heap_[this->size_] >= this->heap_[parent])
      this->shift_up(pos);
    else
      this->shift_down(pos);
    return val;
  }
  void shift_up(int pos)
  {
    int val = this->heap_[pos];

    int lchild = (pos * 2) + 1;
    while (lchild < this->size_)
    {
      if ((lchild + 1) < this->size_ 
          && this->heap_[lchild] > this->heap_[lchild + 1])
        ++lchild;
      if (val <= this->heap_[lchild])
        break;
      else
      {
        this->heap_[pos] = this->heap_[lchild];
        pos = lchild;
        lchild = (2 * lchild) + 1;
      }
    }
    this->heap_[pos] = val;
  }
  void dump()
  {
    printf("heap data : ");
    for (int i = 0; i < this->size_; ++i)
      printf("%d ", this->heap_[i]);
    printf("\n\n");
  }
private:
  int *heap_;
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

  printf("prepop someone : ");
  for (int i = 0; i < 100; ++i)
  {
    printf("%d ", h.pop(rand() % 100));
  }
  printf("\n\n");

  // print
  printf("pop heap : ");
  for (int i = 0; i < 100; ++i)
  {
    printf("%d ", h.pop());
  }
  printf("\n\n");
}
