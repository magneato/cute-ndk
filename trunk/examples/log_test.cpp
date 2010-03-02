#include <ndk/logger.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <assert.h>

ndk::logger *main_log = ndk::log_manager::instance()->get_logger("root.main");

ndk::logger *long_str = ndk::log_manager::instance()->get_logger("root.main.longstr");

ndk::logger *sub_log = ndk::log_manager::instance()->get_logger("root.main.test.sub");
ndk::logger *l_sub1 = ndk::log_manager::instance()->get_logger("root.main.test.sub.sub_1");
ndk::logger *l_sub2 = ndk::log_manager::instance()->get_logger("root.main.test.sub.sub_2");
ndk::logger *l_sub3 = ndk::log_manager::instance()->get_logger("root.main.test.sub.sub_3");
ndk::logger *l_sub4 = ndk::log_manager::instance()->get_logger("root.main.test.sub.sub_4");
ndk::logger *l_sub5 = ndk::log_manager::instance()->get_logger("root.main.test.sub.sub_5");

void method_trace_1(ndk::logger *);
void method_trace_2(ndk::logger *);
void method_trace_3(ndk::logger *);
void method_trace_4(ndk::logger *);
void method_trace_5(ndk::logger *);

void method_trace(ndk::logger *log)
{
  METHOD_TRACE(log);
  method_trace_1(log);
}
void method_trace_1(ndk::logger *log)
{
  METHOD_TRACE(log);
  method_trace_2(log);
}
void method_trace_2(ndk::logger *log)
{
  METHOD_TRACE(log);
  method_trace_3(log);
  method_trace_3(log);
}
void method_trace_3(ndk::logger *log)
{
  METHOD_TRACE(log);
  method_trace_4(log);
}
void method_trace_4(ndk::logger *log)
{
  METHOD_TRACE(log);
  method_trace_5(log);
}
void method_trace_5(ndk::logger *log)
{
  METHOD_TRACE(log);
}

ndk::logger *net_log = ndk::log_manager::instance()->get_logger("root.net");
int main(int argc, char *argv[])
{
  //读取配置文件
  if (ndk::log_manager::instance()->init(argv[1]) != 0)
  {
    ::fprintf(stderr, "init failed\n");
    return -1;
  }
  ndk::logger *test_log = ndk::log_manager::instance()->get_logger("root.main.test");
  if (test_log == 0) 
  {
    fprintf(stderr, "get test logger failed\n");
    return -1;
  }

  char longstr[1024] = {0};
  for(int i = 0; i < 1023; ++i)
  {
    longstr[i] = 'a' + i % 26;
  }
  method_trace(main_log);

  ndk::logger *root_log = ndk::log_manager::instance()->get_logger("root");
  root_log->log(LL_FATAL, "hello root logger : fatal");
  root_log->log(LL_DEBUG, "hello root logger : debug");
  //下面这行没实际意义
  ndk::logger *root_log_clone = ndk::log_manager::instance()->get_logger("root");
  assert(root_log == root_log_clone);

  int count = 0;
  //对象sub_l
  ndk::logger *sub_log = 0;
  while (1)
  {
    //sub_l对象没有初始化,因此不会写日志文件
    if (sub_log)
    {
      sub_log->log(LL_TRACE, "hello logger ----- %s", "aaaaaa");
      sub_log->log(LL_DEBUG, "hello logger debug %s", "aaaaaa");
      sub_log->log(LL_WNING, "hello logger wning %s", "aaaaaa");
      sub_log->log(LL_ERROR, "hello logger erro %s", "aaaaaa");
      sub_log->log(LL_RINFO, "hello logger rinfo %s", "aaaaaa");
      sub_log->log(LL_FATAL, "hello logger fatal %s", "aaaaaa");
    }

    if (count++ > 10 && sub_log == 0)
    {
      sub_log = ndk::log_manager::instance()->get_logger("root.main.test.sub");
    }

    if (count > 20 && sub_log)
    {
      sub_log->shutdown(true);
    }

    root_log->log(LL_TRACE, "hello logger trace %d", 222222);
    root_log->log(LL_DEBUG, "hello logger debug %d", 222222);
    root_log->log(LL_WNING, "hello logger wning %d", 222222);
    root_log->log(LL_ERROR, "hello logger error %d", 222222);
    root_log->log(LL_RINFO, "hello logger rinfo %d", 222222);
    root_log->log(LL_FATAL, "hello logger fatal %d", 222222);

    main_log->trace("hello logger trace %d", 333333);
    main_log->debug("hello logger debug %d", 333333);
    main_log->warning("hello logger wning %d", 333333);
    main_log->error("hello logger error %d", 333333);
    main_log->rinfo("hello logger rinfo %d", 333333);
    main_log->fatal("hello logger fatal %d", 333333);

    LOG_TRACE(main_log, "hello logger trace " << 444444);
    LOG_DEBUG(main_log, "hello logger debug " << 444444);
    LOG_WNING(main_log, "hello logger wning " << 444444);
    LOG_RINFO(main_log, "hello logger rinfo " << 444444);
    LOG_FATAL(main_log, "hello logger fatal " << 444444);

    long_str->log(LL_TRACE, "long str [%s]", longstr);

    l_sub1->log(LL_TRACE, "hello logger %d", 999);
    l_sub2->log(LL_FATAL, "hello logger %d", 999);
    l_sub2->log(LL_ERROR, "hello logger %d", 999);
    l_sub3->log(LL_DEBUG, "hello logger %d", 999);
    l_sub4->log(LL_WNING, "hello logger %d", 999);
    l_sub5->log(LL_RINFO, "hello logger %d", 999);
    usleep(100*1000);
  }

  return 0;
}
