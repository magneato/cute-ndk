// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-21 21:57
 * Brief    : For testing <epoll_reactor>, <acceptor>.
 */
//========================================================================

#include <string>
#include <ndk/cache_heap.h>

int main ()
{
  char *p = new char[512];
  memset(p, 0, 512);
  ndk::lru_cache_object_factory fct;
  ndk::cache_heap<std::string> hp(1024);
  hp.insert("cui", fct.create(new int(10), 4, 0));
  hp.insert("shao", fct.create(new int(10), 4, 0));
  hp.insert("wei", fct.create(new int(10), 4, 0));
  hp.insert("cuis", fct.create(new int(10), 4, 0));
  hp.insert("cuish", fct.create(new int(10), 4, 0));
  hp.insert("cuisha", fct.create(new int(10), 4, 0));
  hp.insert("cuishao", fct.create(new int(10), 4, 0));
  delete []p;
}
