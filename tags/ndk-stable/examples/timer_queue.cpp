#include "ndk/timer_queue.h"
#include "ndk/date_time.h"
#include "ndk/reactor.h"
#include "ndk/epoll_reactor.h"
#include "ndk/logger.h"

#include <map>
#include <signal.h>
#include <stdlib.h>

class heart_beat;

static ndk::logger *log = ndk::log_manager::instance()->get_logger("root");
// not lock protect
class heart_beat : public ndk::event_handler
{
public:
  heart_beat()
    : terminate_(0),
      count_(0),
      timer_id_(-1)
  {}
  ~heart_beat()
  {
  }
  int handle_timeout(const void *arg, const ndk::time_value &curr_time)
  {
    int sec = curr_time.sec();
    ndk::time_value tv = curr_time;
    tv.sec(0);
    char ts[32] = {0};
    ndk::date_time dt(sec);
    dt.to_str(ts, 32);
    if (this->count_ == 0)
    {
      log->rinfo("timer[%p] - delay = [%s] %dth active in %d.%ld [%s]", 
                 this,
                 (char *)arg,
                 this->count_,
                 sec,
                 tv.msec(),
                 ts);
    }else
    {
      log->rinfo("timer[%p] - delay = [%s] %dth active in %d.%ld [%s]", 
                 this,
                 (char *)arg,
                 this->count_,
                 sec,
                 tv.msec(),
                 ts);
    }
    if (this->count_ == 5)//*((int *)arg))
    {
      ndk::reactor::instance()->cancel_timer(this->timer_id(),
                                             0,
                                             0);
      return 0;
    }
    this->count_++;
    return 0;
  }
  int handle_close (const void *arg, ndk::reactor_mask mask)
  {
    log->rinfo("timer - %d closed", *((int *)arg));
    void *p = const_cast<void *>(arg);
    delete [](char*)p;
    return 0;
  }
  void terminate ()
  {
    this->terminate_ = 1;
  }
  int timer_id()
  {
    return this->timer_id_;
  }
  void timer_id(const int id)
  {
    this->timer_id_ = id;
  }
protected:
  int terminate_;
  int count_;
  int timer_id_;
};
int main ()
{
  if (ndk::log_manager::instance()->init("logger-cfg.ini") != 0)
    return -1;
  ndk::timer_queue *tq = new ndk::timer_queue(4096000, 102400);
  ndk::epoll_reactor<ndk::reactor_null_token> *ep = new ndk::epoll_reactor<ndk::reactor_null_token>();
  if (ep->open(tq) != 0)
  {
    log->rinfo("epoll reactor open failed");
    return -1;
  }
  ndk::reactor::instance(new ndk::reactor(ep));
  tq->set_reactor(ndk::reactor::instance());
  srand(::time(0));
  ndk::time_value stv = ndk::time_value::gettimeofday();
  ndk::time_value mtv = ndk::time_value::gettimeofday();
  mtv.sec(0);
  char ts[32] = {0};
  ndk::date_time().to_str(ts, 32);
  log->rinfo("start time %ld.%ld [%s]", stv.sec(), mtv.msec(), ts);
#if 1
  for (int i = 0; i < 10290; ++i)
  {
    char *p = new char[16];
    int r = rand()%10;
    r += '0';
    std::memcpy(p, &r, sizeof(int));
    heart_beat *hb = new heart_beat();
    int timerid = ndk::reactor::instance()->schedule_timer(hb,
                                                           p,
                                                           ndk::time_value(r - '0', 0));
                                                           //ndk::time_value(3, 0));
    hb->timer_id(timerid);

    log->rinfo("schedule timer[%p] id = %d delay = %d", hb, timerid, r);
  }
#else
  int timerid = -1;
  const char *entry = 0;
  char *arg = 0;
  entry = "2-10 2-10 23-7/2 -/ mon-/2";
  arg = new char[128];
  ::memset(arg, 0, 128);
  ::strcpy(arg, entry);
  timerid = ndk::reactor::instance()->crontab(new heart_beat(), arg, entry);
  log->rinfo("schedule timer[%s] id = %d", entry, timerid);

  entry = "45-10/2 * 23-7/2 * mon-/2";
  arg = new char[128];
  ::memset(arg, 0, 128);
  ::strcpy(arg, entry);
  timerid = ndk::reactor::instance()->crontab(new heart_beat(), arg, entry);
  log->rinfo("schedule timer[%s] id = %d", entry, timerid);

  entry = "12/2 */2 23-30/2 4-10/2,11/2,12- 2,4,7";
  arg = new char[128];
  ::memset(arg, 0, 128);
  ::strcpy(arg, entry);
  timerid = ndk::reactor::instance()->crontab(new heart_beat(), arg, entry);
  log->rinfo("schedule timer[%s] id = %d", entry, timerid);

  entry = "30-10/2 * * * *";
  arg = new char[128];
  ::memset(arg, 0, 128);
  ::strcpy(arg, entry);
  timerid = ndk::reactor::instance()->crontab(new heart_beat(), arg, entry);
  log->rinfo("schedule timer[%s] id = %d", entry, timerid);

  entry = "*/2 */2 * * *";
  arg = new char[128];
  ::memset(arg, 0, 128);
  ::strcpy(arg, entry);
  timerid = ndk::reactor::instance()->crontab(new heart_beat(), arg, entry);
  log->rinfo("schedule timer[%s] id = %d", entry, timerid);

  entry = "* * * * *";
  arg = new char[128];
  ::memset(arg, 0, 128);
  ::strcpy(arg, entry);
  timerid = ndk::reactor::instance()->crontab(new heart_beat(), arg, entry);
  log->rinfo("schedule timer[%s] id = %d", entry, timerid);

  entry = "0 4-6 * * sun";
  arg = new char[128];
  ::memset(arg, 0, 128);
  ::strcpy(arg, entry);
  timerid = ndk::reactor::instance()->crontab(new heart_beat(), arg, entry);
  log->rinfo("schedule timer[%s] id = %d", entry, timerid);
#endif
  ndk::reactor::instance()->run_reactor_event_loop();
}
