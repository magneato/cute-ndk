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
#include <ndk/ndk_os.h>
#include <ndk/ndk_log.h>
#include <ndk/reactor.h>
#include <ndk/acceptor.h>
#include <ndk/date_time.h>
#include <ndk/inet_addr.h>
#include <ndk/epoll_reactor.h>

#include <stdarg.h>
#include <unistd.h>

#include "http_connection.h"

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
void sig_handle()
{
  signal(SIGHUP, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, SIG_IGN);
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
    pid_t childpid = fork();
    if (childpid < 0)      // < 0 is wrong! 
    {
      ndk::date_time().to_str(st, sizeof(st));
      guard_log("[%s] fork failed [%s] !\n",
                st,
                strerror(ndk_os::last_error()));
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
    ndk_os::sleep(time_value(1));
  }
}
static const char *svc_name = "fcached";
static const char *version  = "1.0";
static const char *compile_date = __DATE__;
static const char *compile_time = __TIME__;

// accept http socket connection.
ndk::acceptr<http_connection> g_http_acceptor;

static ndk::ndk_log *main_log = ndk::log_manager::instance()->get_logger("main");
int main(int argc, char *argv[])
{ 
  //
  int listen_port = 8000;
  int in_debug_mode = 0;
  int in_guard_mode = 0;
  char *log_config  = "../logger_config.ini";
  
  setbuf(stderr, 0);

  int c = 0;
  while((c = getopt(argc, argv, "p:l:dg")) != -1)
  {
    switch (c)
    {
    case 'v':        // version
      fprintf(stderr, "%s version %s | Compiled %s %s. %s\n"
              , svc_name
              , version
              , compile_date
              , compile_time
              , DESC
             );
      return 0;
    case 'p':
      listen_port = atoi(optarg);
      break;
    case 'l':
      log_config = optarg;
      break;
    case 'd':
      in_debug_mode = 1;
      break;
    case 'g':
      in_guard_mode = 1;
      break;
    default:
      fprintf(stderr, "Usage: fcached [OPTION...]\n\n");
      fprintf(stderr, "  -v,                          Print program version.\n");
      fprintf(stderr, "  -d,                          Lanuch under debug mode.\n");
      fprintf(stderr, "  -g                           Lanuch under guard mode.\n");
      fprintf(stderr, "  -p number                    Listen port.\n");
      fprintf(stderr, "  -l FILE,                     Logger config file name.\n");
      return 0;
    }
  }
  if (in_debug_mode)
  {
    ndk::daemon(true);
  }else if (in_guard_mode)
  {
    guard_process();
    sig_handle();
  }
  if (ndk::log_manager::instance()->init(log_config) != 0)
  {
    fprintf(stderr, "init logger failed\n");
    return -1;
  }
  ndk::mem_pool::instance()->init(20*1024*1024, 10, 50);
  ndk::epoll_reactor *ep = new ndk::epoll_reactor();
  if (ep->open() != 0)
  {
    main_log->error("open epoll reactor failed");
    return -1;
  }
  ndk::reactor::instance(new ndk::reactor(ep));
  ndk::inet_addr local_addr(listen_port);
  if (g_http_acceptor->open(local_addr, ndk::reactor::instance()) != 0)
  {
    main_log->error("open acceptor failed");
    return -1;
  }
  ndk::reactor::instance()->run_reactor_event_loop();
  return 0;
}
