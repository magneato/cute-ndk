#include <cassert>
#include <iostream>
#include <sys/time.h>
#include "ndk/mem_pool.h"

ndk::mem_pool mp;

class mem_test
{
public:
	mem_test()
	{
	}
	void fun()
	{
		for (int n = 0; n < 1000; ++n)
		{
			assert(n * 23 != 1);
		}
	}
#ifdef USE_POOL
	void *operator new(size_t n)
	{
		return mp.malloc(n);
	}
	void operator delete(void *ptr)
	{
		mp.free(ptr);
	}
#endif
private:
    char cui[1023];
};

int main(int argc ,char *argv[])
{
#ifdef USE_POOL 
	assert(mp.init(2*1024 * 1024, 80, 120) == 0);
#endif

	struct timeval  begin, end;
	::gettimeofday(&begin, NULL);

	int count = 150000;
	mem_test **mem_list = new mem_test*[count];
	for (int i = 0; i < 20; ++i)
	{
		for (int n = 0; n < count; ++n)
		{
			mem_test *pMem = NULL;
			pMem = new mem_test;
			mem_list[n] = pMem;
		}
#ifdef DUMP_INFO
		//sleep(1);
		mp.dump();
#endif
		//for (int n = count - 1; n >= 0; --n)
		for (int n = 0; n < count; ++n)
		{
			mem_list[n]->fun();
			delete mem_list[n];
			mem_list[n] = 0;
		}
	}
#ifdef DUMP_INFO
	mp.dump();
#endif
	//
	::gettimeofday(&end, NULL);

	int sec = end.tv_sec - begin.tv_sec;

	int usec = end.tv_usec - begin.tv_usec;
	if (usec < 0)
	{
		sec--;
		usec += 1000*1000*1000;
	}
	printf("use time: %d.%d\n", 
			sec, usec);

	return 0;
}
