// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-24 22:01
 *
 * Brief    : Fast file cache server
 */
//========================================================================

#include <ndk/ndk.h>
#include <ndk/logger.h>
#include <ndk/reactor.h>
#include <ndk/acceptor.h>
#include <ndk/connector.h>
#include <ndk/date_time.h>
#include <ndk/inet_addr.h>
#include <ndk/cache_manager.h>
#include <ndk/epoll_reactor.h>
#include <ndk/asynch_file_io.h>

#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>

#ifdef DUMP_INFO
# include "pipe_buffer.h"
#endif

#include "pull_agent.h"
#include "http_client.h"
#include "buffer_manager.h"
#include "dispatch_data_task.h"

// global vars.
int g_listen_port = 8000;
int g_concurrency_num = 1000;
std::string g_doc_root = ".";
int g_bandwidth = 1000;
ndk::asynch_file_io **g_aio_task = 0;
dispatch_data_task *g_dispatch_data_task = 0;
int g_aio_task_size = 1;
int g_aio_task_thread_size = 2;
int g_dispatch_data_task_thread_size = 1;
size_t g_max_mem_cache_size = 16*1024*1024;
size_t g_min_mem_cache_size = 32;

ndk::cache_manager<std::string, ndk::thread_mutex> *g_cache_manager = 0;
buffer_manager *file_io_cache_mgr = new buffer_manager(1024*1024*1);
//

static void guard_log(const char *format, ...)
{
  va_list argptr;
  va_start(argptr, format);
  FILE *fp = fopen("debug.log", "a+");
  if (fp == NULL)
    return;
  vfprintf(fp, format, argptr);
  fclose(fp);
}
#ifdef DUMP_INFO
void dump(int )
{
  file_io_cache_mgr->flush();
  g_cache_manager->flush_all();
  g_aio_task[0]->dump();

  pipe_buffer::dump();
}
#endif
void sig_handle()
{
#ifdef DUMP_INFO
  signal(SIGHUP,  dump);
#endif
  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT,  SIG_IGN);
}
void guard_process()
{
  int status;
  pid_t retpid;
  sig_handle();
  //
  while(1)
  {
    char st[32] = {0};
    pid_t childpid = ::fork();
    if (childpid < 0)      // < 0 is wrong! 
    {
      ndk::date_time().to_str(st, sizeof(st));
      guard_log("[%s] fork failed [%s] !\n",
                st,
                strerror(errno));
      ::exit(-1);
    }else 
      if (childpid == 0) return ;
    // parent process !
    if ((retpid = waitpid(childpid, &status, 0)) < 0)
    {
      ndk::date_time().to_str(st, sizeof(st));
      guard_log("[%s] parent catch child : %u failed\n", 
                st,
                childpid);
      exit(-1);
    }
    ndk::date_time().to_str(st, sizeof(st));
    guard_log("[%s] child process : %u exit, status is %d\n",
              st,
              retpid,
              status);
    if (status == 0)
    {
      ndk::date_time().to_str(st, sizeof(st));
      guard_log("[%s] child process : %d exit normally(exit-code = 0),"
                " so server terminate\n",
                st,
                retpid);
      ::exit(0);
    }
    if (WIFEXITED(status)) //is non-zero if the child exited normally
    {
      if (WEXITSTATUS(status) == 127) //check exit code
      {
        guard_log("[%s] child process : %d exit in fatal(exit-code = 127),"
                  " so server terminate\n",
                  st,
                  retpid);
        ::exit(0);
      }
    }else if (WIFSIGNALED(status)) //program exit by a siganl but not catched
    {
      guard_log("[%s] child process : %d exit by signal[%d], "
                "then try to reboot\n",
                st,
                retpid, 
                WTERMSIG(status));
      // goto fork
    }
    ndk::sleep(ndk::time_value(2));
  }
}
static const char *svc_name = "fcached";
static const char *version  = "1.0";
static const char *compile_date = __DATE__;
static const char *compile_time = __TIME__;

// accept http socket connection.
ndk::acceptor<http_client> *g_http_acceptor;
ndk::connector<pull_agent> *g_connector;

void print_usage()
{
  printf("Usage: fcache [OPTION...]\n\n");
  printf("  -c  concurrency       Number of multiple requests to perform at a time.\n"); 
  printf("  -p  port              Listen port(default is 8800)\n");
  printf("  -r  path              Document root(default is current path)\n");
  printf("  -b  kbps              Max of ouput bandwidth\n");
  printf("  -l  path              Logger config name\n");
  printf("  -H  MB                Cache manager high water mark\n");
  printf("  -L  MB                Cache manager low water mark\n");
  printf("  -M  bytes             Max cache file size\n");
  printf("  -m  bytes             Min cache file size\n");
  printf("  -a  number            aio thread number\n");
  printf("  -v                    Output version info\n");
  printf("  -h                    Help info\n");
  printf("\n");
  printf("You can use 'watch curl -s http://localhost:port/' to monitor me.\n\n");
}

//
static ndk::logger *main_log = ndk::log_manager::instance()->get_logger("root");

int main(int argc, char *argv[])
{ 
  if (argc == 1)
  {
    print_usage();
    return 0;
  }
  //
  int in_debug_mode = 0;
  int in_guard_mode = 0;

  int high_water_mark = 0;
  int lower_water_mark = 0;

  int count = 0;
  
  std::string log_config;

  setbuf(stderr, 0);

  // parse args
  opterr = 0;
  int c = -1;
  const char *opt = ":c:p:r:d:l:b:H:L:hvM:m:a:";
  while((c = getopt(argc, argv, opt)) != -1)
  {
    switch (c)
    {
    case 'v':        // version
#ifndef DESC
# define DESC ""
#endif
      fprintf(stderr, "%s version %s | Compiled %s %s. %s\n"
              , svc_name
              , version
              , compile_date
              , compile_time
              , DESC
             );
      return 0;
    case 'c':
      g_concurrency_num = ::atoi(optarg);
      ++count;
      break;
    case 'p':
      g_listen_port = ::atoi(optarg);
      ++count;
      break;
    case 'r':
      g_doc_root = optarg;
      ++count;
      break;
    case 'd':
      in_debug_mode = 1;
      break;
    case 'l':
      log_config = optarg;
      ++count;
      break;
    case 'g':
      in_guard_mode = 1;
      break;
    case 'b':
      g_bandwidth = ::atoi(optarg);
      ++count;
      break;
    case 'H':
      high_water_mark = ::atoi(optarg);
      ++count;
      break;
    case 'L':
      lower_water_mark = ::atoi(optarg);
      ++count;
      break;
    case 'M':
      g_max_mem_cache_size = ::atoi(optarg);
      ++count;
      break;
    case 'm':
      g_min_mem_cache_size = ::atoi(optarg);
      ++count;
      break;
    case 'a':
      g_aio_task_thread_size = ::atoi(optarg);
      ++count;
      break;
    case 'h':
      print_usage();
      return 0;
    case ':':
      fprintf(stderr, "\nOption -%c requires an operand\n\n", optopt);
    case '?':
    default:
      print_usage();
      return 0;
    }
  }
  if (count != 10)
  {
    print_usage();
    return 0;
  }
  if (in_debug_mode)
  {
    ndk::daemon(true);
  }else if (in_guard_mode)
  {
    guard_process();
  }
  sig_handle();
  if (ndk::log_manager::instance()->init(log_config.c_str()) != 0)
  {
    fprintf(stderr, "init logger failed\n");
    return -1;
  }
  ndk::epoll_reactor<ndk::reactor_token> *r_impl
    = new ndk::epoll_reactor<ndk::reactor_token>();
  if (r_impl->open() != 0)
  {
    main_log->error("open epoll reactor failed");
    return -1;
  }
  ndk::reactor::instance(new ndk::reactor(r_impl));

  g_http_acceptor = new ndk::acceptor<http_client>();
  ndk::inet_addr local_addr(g_listen_port);
  if (g_http_acceptor->open(local_addr, ndk::reactor::instance()) != 0)
  {
    main_log->error("open acceptor failed");
    return -1;
  }
  //
  g_connector = new ndk::connector<pull_agent>();
  g_connector->open(ndk::reactor::instance());

  g_cache_manager = 
    new ndk::cache_manager<std::string, ndk::thread_mutex>(g_min_mem_cache_size,
                                                           g_max_mem_cache_size,
                                                           high_water_mark,
                                                           lower_water_mark);
  g_dispatch_data_task = new dispatch_data_task[g_dispatch_data_task_thread_size];
  for (int i = 0; i < g_dispatch_data_task_thread_size; ++i)
    g_dispatch_data_task[i].open();

  g_aio_task = new ndk::asynch_file_io*[g_aio_task_size];
  for (int i = 0; i < g_aio_task_size; ++i)
  {
    g_aio_task[i] = new ndk::asynch_file_io(256);
    g_aio_task[i]->open(g_aio_task_thread_size);
  }

  //
  ndk::reactor::instance()->run_reactor_event_loop();
  main_log->error("reactor exit! [%s]", strerror(errno));
  return 0;
}
