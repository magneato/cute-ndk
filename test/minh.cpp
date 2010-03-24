#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <cassert>

#include "ndk/cache_heap.h"
#include "ndk/min_heap.h"

class heap_item
{
public:
  heap_item(int val)
  : value_(val),
  heap_idx_(-1)
  { }

  inline bool operator < (const heap_item &hi)
  { return this->value_ < hi.value_; }
  inline bool operator <= (const heap_item &hi)
  { return this->value_ <= hi.value_; }
  inline bool operator > (const heap_item &hi)
  { return this->value_ > hi.value_; }
  inline bool operator >= (const heap_item &hi)
  { return this->value_ >= hi.value_; }

  int value_;
  int heap_idx_;
};
int main ()
{
  srand(time(0));

  ndk::min_heap<heap_item> h(1024*2);
  for (int i = 0; i < 400; ++i)
  {
    h.push(new heap_item(rand() % 1000));
  }

  h.check_heap();
#if 1
  printf("prepop someone : ");
  for (int i = 0; i < 200; ++i)
  {
    int pos = rand() % 200;
    printf("%d-%d ", pos, h.pop(pos)->value_);//h.pop(rand() % 100));
  }
  printf("\n\n");
  h.check_heap();

  for (int i = 0; i < 200; ++i)
  {
    h.push(new heap_item(rand() % 1000));
  }
  h.check_heap();
#endif

  h.check_heap();
  // print
  printf("pop heap : ");
  for (int i = 0; i < 400; ++i)
  {
    heap_item *item = h.pop(0);
    printf("%d ", item->value_);
  }
  printf("\n\n");
}
