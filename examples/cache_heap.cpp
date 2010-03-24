// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-21 21:57
 * Brief    : For testing <epoll_reactor>, <acceptor>.
 */
//========================================================================

#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <ndk/cache_heap.h>

int main ()
{
  srand(time(0));

  ndk::lru_cache_object_factory fct;
  ndk::cache_heap<std::string> hp(1024);
  ndk::cache_object *item = 0;

  for (int c = 0; c < 1; ++c)
  {
    for (int i = 0; i < 200; ++i)
    {
      item = fct.create(new int(10), 4, 0);
      item->acquire();
      hp.insert("cui", item);
      usleep((rand() % 200)*1000);
    }
    hp.dump();
  }
  return 0;
}
