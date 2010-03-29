#include "ndk/timer_queue.h"
#include "ndk/strace.h"
#include "ndk/config.h"
#include "ndk/reactor.h"
#include "ndk/date_time.h"
#include "ndk/unused_arg.h"
#include "ndk/event_handler.h"

#include <limits>
#include <cassert>
#include <string.h>
#include <ctype.h>
#include <algorithm>

namespace ndk
{

static const char *MONTH_LIST[] = {"jan", "feb", "mar", "apr", "may", 
  "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
static const char *WDAY_LIST[]  = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};

// For crontab
#define FIRST_MINUTE   0
#define LAST_MINUTE    59
#define MINUTE_COUNT   60

#define FIRST_HOUR     0
#define LAST_HOUR      23
#define HOUR_COUNT     24

#define FIRST_MDAY     1
#define LAST_MDAY      31
#define MDAY_COUNT     32

#define FIRST_MONTH    1
#define LAST_MONTH     12
#define MONTH_COUNT    13

#define FIRST_WDAY     0
#define LAST_WDAY      7
#define WDAY_COUNT     8

#define GENERAL_TIMER  0x11
#define CRONTAB_TIMER  0x12
enum 
{
  CRON_MIN  = 0,
  CRON_HOUR,   
  CRON_MDAY, 
  CRON_MONTH,
  CRON_WDAY,

  CRON_ENTRY_NUM
};

#define PREALLOC_TIMER_NODE_NUM   1024

#define NDK_HEAP_PARENT(X)   (X == 0 ? 0 :(((X) - 1) / 2))
#define NDK_HEAP_LCHILD(X)   (((X)+(X))+1)

timer_node::timer_node()
: timer_id_(-1),
  type_(GENERAL_TIMER),
  crontab_entry_(0),
  handler_(0),
  arg_(0),
  timer_value_(0),
  interval_(0),
  prev_(0),
  next_(0)
{ 
}
timer_node::~timer_node()
{
  this->release_cron_entry();
}
void timer_node::release_cron_entry()
{
  if (this->crontab_entry_)
  {
    for (int i = 0; i < CRON_ENTRY_NUM; ++i)
      delete []this->crontab_entry_[i];
    delete []this->crontab_entry_;
    this->crontab_entry_ = 0;
  }
}
void timer_node::set(int timer_id,
                     const event_handler *handler,
                     const void *arg,
                     const time_value &t,
                     const time_value &i,
                     timer_node *p/* = 0*/,
                     timer_node *n/* = 0*/)
{
    this->timer_id_ = timer_id;
    this->handler_  = const_cast<event_handler *>(handler);
    this->arg_      = arg;
    this->timer_value_ = t;
    this->interval_ = i;
    this->prev_     = p;
    this->next_     = n;
}
char **timer_node::alloc_cron_entry()
{
  if (this->crontab_entry_ == 0)
  {
    this->crontab_entry_ = new char*[CRON_ENTRY_NUM];
    this->crontab_entry_[CRON_MIN] = new char[MINUTE_COUNT];
    this->crontab_entry_[CRON_HOUR] = new char[HOUR_COUNT];
    this->crontab_entry_[CRON_MDAY] = new char[MDAY_COUNT];
    this->crontab_entry_[CRON_MONTH] = new char[MONTH_COUNT];
    this->crontab_entry_[CRON_WDAY] = new char[WDAY_COUNT];
  }
  std::memset(this->crontab_entry_[CRON_MIN], 0, MINUTE_COUNT);
  std::memset(this->crontab_entry_[CRON_HOUR], 0, HOUR_COUNT);
  std::memset(this->crontab_entry_[CRON_MDAY], 0, MDAY_COUNT);
  std::memset(this->crontab_entry_[CRON_MONTH], 0, MONTH_COUNT);
  std::memset(this->crontab_entry_[CRON_WDAY], 0, WDAY_COUNT);
  return this->crontab_entry_;
}
timer_node &timer_node::operator = (const timer_node &tn)
{
    this->timer_id_ = tn.timer_id_;
    this->type_     = tn.type_;
    this->handler_  = tn.handler_;
    this->arg_	    = tn.arg_;
    if (tn.crontab_entry_ && this->crontab_entry_)
    {
      std::memcpy(this->crontab_entry_[CRON_MIN], 
                  tn.crontab_entry_[CRON_MIN],
                  MINUTE_COUNT);
      std::memcpy(this->crontab_entry_[CRON_HOUR], 
                  tn.crontab_entry_[CRON_HOUR],
                  HOUR_COUNT);
      std::memcpy(this->crontab_entry_[CRON_MDAY], 
                  tn.crontab_entry_[CRON_MDAY],
                  MDAY_COUNT);
      std::memcpy(this->crontab_entry_[CRON_MONTH], 
                  tn.crontab_entry_[CRON_MONTH],
                  MONTH_COUNT);
      std::memcpy(this->crontab_entry_[CRON_WDAY], 
                  tn.crontab_entry_[CRON_WDAY],
                  WDAY_COUNT);
    }
    this->timer_value_ = tn.timer_value_;
    this->interval_ = tn.interval_;
    this->prev_	    = tn.prev_;
    this->next_	    = tn.next_;
    return *this;
}
// ---------------------------------------------------------------------
timer_queue::timer_queue(int max_size, int pre_alloc_num/* = 1024*/)
: max_size_(max_size),
  curr_size_(0),
  timer_heap_size_(0),
  timer_heap_(0),
  free_timer_nodes_(0),
  free_crontab_timers_(0),
  crontab_timers_(0),
  timer_ids_(0),
  timer_ids_size_(0),
  min_timer_id_(0)
{
  if (max_size > std::numeric_limits<int>::max())
  {
    max_size = std::numeric_limits<int>::max();
    max_size_ = max_size;
  }
  init(pre_alloc_num);
}
timer_queue::timer_queue()
: max_size_(1000000),
  curr_size_(0),
  timer_heap_size_(0),
  timer_heap_(0),
  free_timer_nodes_(0),
  free_crontab_timers_(0),
  crontab_timers_(0),
  timer_ids_(0),
  timer_ids_size_(0),
  min_timer_id_(0)
{
    init(PREALLOC_TIMER_NODE_NUM);
}
timer_queue::~timer_queue()
{
  // Clean up all the nodes still in the queue
  for (int i = 0; i < this->curr_size_; ++i)
    this->free_node(this->timer_heap_[i]);

  delete [] this->timer_heap_;
  this->timer_heap_ = 0;

  for (timer_node *iter = this->free_timer_nodes_; iter != 0; )
  {
    timer_node *p = iter;
    iter = iter->next();
    delete p;
  }
  this->free_timer_nodes_ = 0;

  for (timer_node *iter = this->free_crontab_timers_; iter != 0; )
  {
    timer_node *p = iter;
    iter = iter->next();
    delete p;
  }
  this->free_crontab_timers_ = 0;

  if (this->timer_ids_)
    delete [] this->timer_ids_;
  this->timer_ids_ = 0;
}
void timer_queue::init(int pre_alloc_num)
{
  STRACE("");
  // Create the heap array.
  this->timer_heap_ = new timer_node* [pre_alloc_num];
  this->timer_ids_  = new int[pre_alloc_num];
  for (int i = 0; i < pre_alloc_num; ++i)
  {
    this->timer_heap_[i] = 0;
    this->timer_ids_[i]  = -1;
  }
  timer_node *iter = 0;
  for (int i = 0; i < pre_alloc_num; ++i)
  {
    timer_node *node = new timer_node;
    if (this->free_timer_nodes_ == 0) 
    {
      this->free_timer_nodes_ = node;
      iter = this->free_timer_nodes_;
    }else
    {
      iter->next(node);
      iter = iter->next();
    }
  }

  this->timer_heap_size_ = pre_alloc_num;
  this->timer_ids_size_  = pre_alloc_num;
  this->min_timer_id_ = 0;
}
int timer_queue::alloc_timerid()
{
  STRACE("");
  for (int i = this->min_timer_id_; i < this->timer_ids_size_; ++i)
  {
    if (this->timer_ids_[i] == -1)
    {
      this->min_timer_id_ += 1;
      return i;
    }
  } 
  return -1;
}
void timer_queue::release_timerid(int id)
{
  STRACE("");
  if (id >= 0 && id < this->timer_ids_size_)
  {
    this->timer_ids_[id] = -1;
    this->min_timer_id_ = std::min(id, this->min_timer_id_);
  }
  return ;
}
time_value* timer_queue::calculate_timeout(const time_value* max_wait_time)
{
  STRACE("");

  if (this->is_empty())
    return const_cast<time_value*>(max_wait_time);

  const time_value cur_time = time_value::gettimeofday();
  if (this->earliest_time() > cur_time)
  {
    // The earliest item on the timer_queue is still in the
    // future.  Therefore, use the smaller of(1) caller's wait
    // time or(2) the delta time between now and the earliest
    // time on the timer_queue.
    this->timeout_ = this->earliest_time() - cur_time;
    if (max_wait_time == 0 || *max_wait_time > this->timeout_)
      return &this->timeout_;

    return const_cast<time_value*>(max_wait_time);
  }
  // The earliest item on the timer_queue is now in the past.
  // Therefore, we've got to "poll" the Reactor, i.e., it must
  // just check the descriptors and then dispatch timers, etc.
  this->timeout_ = time_value::zero;
  return &this->timeout_;
}
time_value* timer_queue::calculate_timeout(const time_value* max_wait_time,
                                           time_value* the_timeout)
{
  STRACE("");
  if (the_timeout == 0) return 0; 

  if (this->is_empty())
  {
    if (max_wait_time)
      *the_timeout = *max_wait_time;
    else
      return 0;
  }else
  {
    const time_value cur_time = time_value::gettimeofday();
    if (this->earliest_time() > cur_time)
    {
      // The earliest item on the timer_queue is still in the
      // future.  Therefore, use the smaller of(1) caller's wait
      // time or(2) the delta time between now and the earliest
      // time on the timer_queue.
      *the_timeout = this->earliest_time() - cur_time;
      if (!(max_wait_time == 0 || *max_wait_time > *the_timeout))
        *the_timeout = *max_wait_time;
    }else
    {
      // The earliest item on the timer_queue is now in the past.
      // Therefore, we've got to "poll" the Reactor, i.e., it must
      // just check the descriptors and then dispatch timers, etc.
      *the_timeout = time_value::zero;
    }
  }
  return the_timeout;
}
int timer_queue::schedule_i(const event_handler *eh, 
                            const void *arg,
                            const time_value &delay_time,
                            const time_value &interval_time)
{
  STRACE("");
  if (this->curr_size_ >= this->max_size_)
    return -1;

  timer_node *node = 0;
  node = this->alloc_node(GENERAL_TIMER);
  assert(node != 0);
  if (node == 0) return -1;

  int timer_id = this->alloc_timerid();
  if (timer_id == -1) 
  {
    assert(timer_id != -1);
    this->free_node(node);
    return -1;
  }

  node->set(timer_id, eh, arg, delay_time, interval_time);

  node->type(GENERAL_TIMER);
  this->insert(node);

  this->verify_heap();
  return timer_id;
}
namespace ndk_help
{
  inline void replace(char *en, const char *str, int n)
  {
    char *p = ::strstr(en, str);
    if (p == 0) return ;
    assert(n < 100);
    do
    {
      int len = ::strlen(en);
      if (n < 10) 
      { 
        *p = '0' + n; 
        ::strcpy(p + 1, p + 3); 
        en[len - 2] = '\0';
      }else
      {
        int h = n / 10;
        int l = n % 10;
        *p = '0' + h; 
        *(p + 1) = '0' + l; 
        ::strcpy(p + 2, p + 3);
        en[len - 1] = '\0';
      }
    }while ((p = ::strstr(en, str)));
  }
}
int timer_queue::handle_timeout(const void * /*arg*/, 
                                const time_value & /*curent_time*/)
{
  if (this->crontab_timers_)
  {
    time_value tv = time_value::gettimeofday();
    date_time now(tv.sec());
    timer_node *iter = this->crontab_timers_;
    for (; iter != 0; )
    {
      char **entry = iter->crontab_entry();
      timer_node *node = iter;
      iter = iter->next();
      if (entry == 0) continue;
      if (entry[CRON_MIN][now.min()]
          && entry[CRON_HOUR][now.hour()]
          && entry[CRON_MDAY][now.mday()]
          && entry[CRON_MONTH][now.month()]
          && entry[CRON_WDAY][now.wday()])
      {
        event_handler *eh = node->handler();
        if (eh)
        {
          if (eh->handle_timeout(node->arg(), tv) != 0)
            eh->handle_close(NDK_INVALID_HANDLE, event_handler::timer_mask);
        }
      }
    }
  }
  return 0;
}
int timer_queue::crontab(const event_handler *eh, 
                         const void *arg, 
                         const char *entry)
{
  if (eh == 0 || entry == 0) return -1;

  if (this->crontab_timers_ == 0)
  {
    date_time dt;
    time_value delay;
    if (dt.sec() > 5)
      delay.set(60 - dt.sec(), 0);
    time_value interval(60, 0);
    if (this->get_reactor()->schedule_timer(this,
                                            0,
                                            delay,
                                            interval) != 0)
      return -1;
  }

  timer_node *node = 0;
  node = this->alloc_node(CRONTAB_TIMER);
  assert(node != 0);
  if (node == 0) return -1;

  int timer_id = this->alloc_timerid();
  if (timer_id == -1)
  {
    assert(timer_id != -1);
    this->free_node(node);
    return -1;
  }
  node->alloc_cron_entry();
  char **cron_entry = node->crontab_entry();
  if (cron_entry == 0)
  {
    assert(cron_entry != 0);
    this->free_node(node);
    return -1;
  }

  node->set(timer_id, eh, arg, 
            time_value::gettimeofday(), 
            time_value::gettimeofday());
  int result = this->parse_cron_entry(entry, cron_entry);
  if (result == -1)
  {
    this->free_node(node);
    return -1;
  }
  node->type(CRONTAB_TIMER);
  this->insert(node);
#if 1
  printf("minute: ");
  for (int i = FIRST_MINUTE; i < LAST_MINUTE; ++i)
    printf("%d ", cron_entry[CRON_MIN][i]);
  printf("\n");

  printf("hour: ");
  for (int i = FIRST_HOUR; i < LAST_HOUR; ++i)
    printf("%d ", cron_entry[CRON_HOUR][i]);
  printf("\n");

  printf("mday: ");
  for (int i = FIRST_MDAY; i < LAST_MDAY; ++i)
    printf("%d ", cron_entry[CRON_MDAY][i]);
  printf("\n");

  printf("month: ");
  for (int i = FIRST_MONTH; i < LAST_MONTH; ++i)
    printf("%d ", cron_entry[CRON_MONTH][i]);
  printf("\n");

  printf("wday: ");
  for (int i = FIRST_WDAY; i < LAST_WDAY; ++i)
    printf("%d ", cron_entry[CRON_WDAY][i]);
  printf("\n");
#endif
  return timer_id;
}
#define _ADVANCE(p)                         \
  while (!isspace(*p)) ++p;                 \
  *p = '\0';                                \
  while (isspace(*p) || *p == '\0') ++p;    \
  *(p - 1) = '\0'

int timer_queue::parse_cron_entry(const char *en, char **crontab_entry)
{
  char *entry = 0;
  int pos = 0;
  while (isspace(en[pos]) && en[pos]) ++pos;
  if (en[pos] == '@')
  {
    if (::strchr((char *)(en + 1), '@'))
      return -1;
    int l = sizeof("* * * * *") - 1;
    entry = ::strdup("* * * * *");
    std::memset(entry, 0, l);

    if (::strcasecmp(entry, "@yearly") == 0)
      ::strcpy(entry, "0 0 1 1 *");
    if (::strcasecmp(entry, "@monthly") == 0)
      ::strcpy(entry, "0 0 1 * *");
    if (::strcasecmp(entry, "@weekly") == 0)
      ::strcpy(entry, "0 0 * * 0");
    if (::strcasecmp(entry, "@daily") == 0)
      ::strcpy(entry, "0 0 * * *");
    if (::strcasecmp(entry, "@hourly") == 0)
      ::strcpy(entry, "0 0 * * *");
    entry[l] = '\0';
  }else
  {
    int l = ::strlen(en) + 1;
    entry = ::new char[l];
    std::memset(entry, 0, l);
    ndk::strstrip(en, entry, l - 1);
    if (::strlen(entry) < (sizeof("* * * * *") - 1)) { ::delete []entry; return -1; }
    char *p1 = entry;
    while ((*p1) != '\0') { *p1 = tolower(*p1); ++p1; }
    // convert string to numberic
    for (size_t i = 0; i < sizeof(MONTH_LIST)/sizeof(MONTH_LIST[0]); ++i)
      ndk_help::replace(entry, MONTH_LIST[i], i + 1);
    for (size_t i = 0; i < sizeof(WDAY_LIST)/sizeof(WDAY_LIST[0]); ++i)
      ndk_help::replace(entry, WDAY_LIST[i], i);
  }
  char *min, *hour, *mday, *month, *wday, *p;
  p = entry; min = entry;
  _ADVANCE(p); hour = p; 
  _ADVANCE(p); mday = p; 
  _ADVANCE(p); month = p; 
  _ADVANCE(p); wday = p;
  
  int result = 0;
  result = this->parse_cron_entry_i(min, crontab_entry[CRON_MIN],
                                    FIRST_MINUTE, LAST_MINUTE, MINUTE_COUNT);
  if (result != 0) goto END;
  result = this->parse_cron_entry_i(hour, crontab_entry[CRON_HOUR],
                                    FIRST_HOUR, LAST_HOUR, HOUR_COUNT);
  if (result != 0) goto END;
  result = this->parse_cron_entry_i(mday, crontab_entry[CRON_MDAY],
                                    FIRST_MDAY, LAST_MDAY, MDAY_COUNT);
  if (result != 0) goto END;
  result = this->parse_cron_entry_i(month, crontab_entry[CRON_MONTH],
                                    FIRST_MONTH, LAST_MONTH, MONTH_COUNT);
  if (result != 0) goto END;
  result = this->parse_cron_entry_i(wday, crontab_entry[CRON_WDAY],
                                    FIRST_WDAY, LAST_WDAY, WDAY_COUNT);
  if (result != 0) goto END;
  if (crontab_entry[CRON_WDAY][FIRST_WDAY]) crontab_entry[CRON_WDAY][LAST_WDAY] = 1;
  if (crontab_entry[CRON_WDAY][LAST_WDAY]) crontab_entry[CRON_WDAY][FIRST_WDAY] = 1;

END:
  ::delete []entry;
  return result;
}
#undef _ADVANCE
namespace ndk_help
{
  int get_list_by_item(const char *entry, char *item,
                       int first, int last, int count)
  {
    ndk::unused_args(count);
    // * or 2
    if (entry[0] == '*')
    {
      for (int i = first; i <= last; ++i) item[i] = 1;
    }else if (isdigit(*entry))
      item[::atoi(entry)] = 1;
    else
      return -1;
    return 0;
  }
  int get_list_by_range(const char *entry, char *item,
                        int first, int last, int count)
  {
    ndk::unused_args(count);
    // 3-5
    int c = 0;
    const char *tp = entry;
    while (*tp) { if (*tp == '-') ++c; ++tp; }
    if (c > 1) return -1;   // 3--4
    char *p = ::strchr((char *)entry, '-');
    if (entry == p) // -4
    {
      int n = ::atoi(entry + 1);
      for (int i = first; i <= n; ++i) item[i] = 1;
    }else if (*(p + 1) == '\0') // 4-
    {
      int n = ::atoi(entry);
      for (int i = n; i <= last; ++i) item[i] = 1;
    }else // 3-5 5-3
    {
      int b = -1, e = -1;
      ::sscanf(entry, "%d-%d", &b, &e);
      if (b > last || b < first) { return -1; }
      if (e > last || e < first) { return -1; }
      if (b > e)  // 50-10
      {
        for (int i = b; i <= last; ++i) item[i] = 1;
        for (int j = first; j <= e; ++j) item[j] = 1;
      }else  // 10-50 10-10
        for (int i = b; i <= e; ++i) item[i] = 1;
    }
    return 0;
  }
  int get_list_by_range_and_step(const char *entry, char *item,
                                 int first, int last, int count)
  {
    const char *tp = entry;
    int c = 0;
    while (*tp) { if (*tp == '-') ++c; ++tp; }
    if (c > 1) return -1;   // 3--4

    c = 0;
    tp = entry;
    while (*tp) { if (*tp == '/') ++c; ++tp; }
    if (c > 1) return -1;   // 3//4

    // 3-9/2
    if (entry[0] == '/') return -1;

    int b = -1, e = -1, step = -1;
    char *p1 = ::strrchr((char *)entry, '-');
    char *p2 = ::strrchr((char *)entry, '/');

    if (p1 != entry)  // 10-20/2 10-/2 10-/
    {
      b = ::atoi(entry);
      if (*(p1 + 1) == '/')
        e = last;
      else
        e = ::atoi(p1 + 1);
    }else              // -20/2 -20/ -/
    {
      if (*(p1 + 1) == '/')
      {
        b = first; e = last;
      }else
        e = ::atoi(p1 + 1);
    }
    if (*(p2 + 1) == '\0')  // step is empty
      step = 1;
    else
      step = ::atoi(p2 + 1);
    if (step == 0) return -1;

    //printf("%s : b = %d e %d step %d\n", entry, b, e, step);
    if (b > last || b < first) { return -1; }
    if (e > last || e < first) { return -1; }
    if (step > last || step < first) { return -1; }
    if (b > e)  // 50-10/2
    {
      int i = 0;
      for (i = b; i <= last; i += step) item[i] = 1;
      i = i % count + first;
      for (int j = i; j <= e; j += step) item[j] = 1;
    }else  // 10-50/2  10-10/2
      for (int i = b; i <= e; i += step) item[i] = 1;
    return 0;
  }
  int get_list_by_step(const char *entry, char *item,
                       int first, int last, int count)
  {
    ndk::unused_args(count);
    const char *tp = entry;
    int c = 0;
    while (*tp) { if (*tp == '/') ++c; ++tp; }
    if (c > 1) return -1;   // 3//4

    // */2 or 3/2
    char *p = ::strchr((char *)entry, '/');
    if (entry[0] == '*' && (p - entry) != 1) return -1;
    if (entry[0] == '*')
    {
      int step = ::atoi(entry + 2);
      if (step == 0) return -1;
      for (int i = first; i <= last; i += step) item[i] = 1;
      return 0;
    }
    //  3/ 3/2
    int b = -1;
    ::sscanf(entry, "%d", &b);
    if (b > last || b < first) return -1;
    item[b] = 1;
    return 0; 
  }
  int get_list(const char *entry, char *item,
               int first, int last, int count)
  {
    // 1,2,5-8,10,20-40/2
    char *p, *bf, *token, *saveptr;
    int result = 0;
    bf = ::strdup(entry);
    for (p = bf; ; p = NULL)
    {
      token = ::strtok_r(p, ",", &saveptr); 
      if (token)
      {
        if (::strchr(token, '-') && ::strchr(token, '/'))
          result = get_list_by_range_and_step(token, item, first, last, count);
        else if (::strchr(token, '-'))
          result = get_list_by_range(token, item, first, last, count);
        else if (::strchr(token, '/'))
          result = get_list_by_step(token, item, first, last, count);
        else
          result = get_list_by_item(token, item, first, last, count);
        if (result != 0) { ::free(bf); return result; }
      }else
        break;
    }
    ::free(bf);
    return 0;
  }
}
int timer_queue::parse_cron_entry_i(const char *entry, char *item,
                                    int first, int last, int count)
{
  int result = 0;
  if (::strchr(entry, ','))
    result = ndk_help::get_list(entry, 
                                item, 
                                first, 
                                last, 
                                count);
  else if (::strchr(entry, '-') && ::strchr(entry, '/')) // 3-9/2  
    result = ndk_help::get_list_by_range_and_step(entry, 
                                                  item, 
                                                  first, 
                                                  last, 
                                                  count);
  else if (::strchr(entry, '-'))
    result = ndk_help::get_list_by_range(entry, 
                                         item, 
                                         first, 
                                         last, 
                                         count);
  else if (::strchr(entry, '/'))
    result = ndk_help::get_list_by_step(entry, 
                                        item, 
                                        first, 
                                        last, 
                                        count);
  else
    result = ndk_help::get_list_by_item(entry, 
                                        item, 
                                        first, 
                                        last, 
                                        count);
  return result; 
}
void timer_queue::insert(timer_node *new_node)
{
  STRACE("");
  if (new_node->type() == GENERAL_TIMER)
  {
    if ((this->curr_size_ + 1) >= this->timer_heap_size_)
      this->grow_heap();

    this->reheap_up(new_node,
                    this->curr_size_,
                    NDK_HEAP_PARENT(this->curr_size_));
    this->curr_size_++;
  }else if (new_node->type() == CRONTAB_TIMER)
  {
    if (this->crontab_timers_ == 0)
    {
      new_node->next(0);
      new_node->prev(0);
      this->crontab_timers_ = new_node;
    }else
    {
      new_node->prev(0);
      this->crontab_timers_->prev(new_node);
      new_node->next(this->crontab_timers_);
      this->crontab_timers_ = new_node;
    }
  }else
  {
    assert(0);
  }
}
void timer_queue::grow_heap()
{
  STRACE("");
  int new_size = std::min(this->curr_size_ * 2, this->max_size_);

  timer_node **new_heap = 0;
  new_heap = new timer_node*[new_size];
  std::memcpy(new_heap, this->timer_heap_, this->curr_size_*sizeof(timer_node *));
  delete [] this->timer_heap_;
  this->timer_heap_ = new_heap;
  this->timer_heap_size_ = new_size;

  // grow the array of timer ids.
  int *new_timer_ids = 0;
  new_timer_ids = new int[new_size];
  std::memcpy(new_timer_ids, this->timer_ids_, this->curr_size_*sizeof(int));
  delete [] timer_ids_;
  this->timer_ids_ = new_timer_ids;
  this->timer_ids_size_ = new_size;

  for (int i = this->curr_size_; i < new_size; ++i)
  {
    this->timer_heap_[i] = 0;
    this->timer_ids_[i] = -1;
  }

  this->min_timer_id_ = this->curr_size_;
}
void timer_queue::reheap_up(timer_node *moved_node,
                            int slot,
                            int parent)
{
  STRACE("");
  // Restore the heap property after an insertion.
  while (slot > 0)
  {
    // If the parent node is greater than the <moved_node> we need
    // to copy it down.
    if (moved_node->timer_value() < this->timer_heap_[parent]->timer_value())
    {
      this->copy(slot, this->timer_heap_[parent]);
      slot = parent;
      parent = NDK_HEAP_PARENT(slot);
    }else
      break;
  }
  // Insert the new node into its proper resting place in the heap and
  // update the corresponding slot in the parallel <timer_ids> array.
  this->copy(slot, moved_node);
}
void timer_queue::reheap_down(timer_node *moved_node,
                              int slot,
                              int child)
{
  STRACE("");
  // Restore the heap property after a deletion.
  while (child < this->curr_size_)
  {
    // Choose the smaller of the two children.
    if (child + 1 < this->curr_size_
        && this->timer_heap_[child + 1]->timer_value()
        < this->timer_heap_[child]->timer_value())
      ++child;
    // Perform a <copy> if the child has a larger timeout value than
    // the <moved_node>.
    if (this->timer_heap_[child]->timer_value() < moved_node->timer_value())
    {
      this->copy(slot, this->timer_heap_[child]);
      slot = child;
      child = NDK_HEAP_LCHILD(child);
    }else
      // We've found our location in the heap.
      break;
  }
  this->copy(slot, moved_node);
}
void timer_queue::copy(int slot, timer_node *moved_node)
{
  // Insert <moved_node> into its new location in the heap.
  this->timer_heap_[slot] = moved_node;
  assert(moved_node->timer_id() >= 0
         && moved_node->timer_id() < this->max_size_);

  // Update the corresponding slot in the parallel <timer_ids_> array.
  this->timer_ids_[moved_node->timer_id()] = slot;
}
void timer_queue::reschedule(timer_node *expired)
{
  this->insert(expired);
}
timer_node *timer_queue::alloc_node(int type)
{
  STRACE("");
  timer_node *node = 0;
  if (type == GENERAL_TIMER)
  {
    if (this->free_timer_nodes_ == 0)
    {
      node = new timer_node;
    }else
    {
      node = this->free_timer_nodes_;
      this->free_timer_nodes_ = node->next();
      node->next(0);
    }
  }else if (type == CRONTAB_TIMER)
  {
    if (this->free_crontab_timers_ == 0)
      node = new timer_node;
    else
    {
      node = this->free_crontab_timers_;
      this->free_crontab_timers_ = node->next();
      node->next(0);
    }
  }
  node->type(type);
  return node;
}
void timer_queue::free_node(timer_node *node)
{
  STRACE("");

  if (node == 0) return ;
  this->release_timerid(node->timer_id());

  node->release_cron_entry();
  if (node->type() == GENERAL_TIMER)
  {
    node->next(this->free_timer_nodes_);
    this->free_timer_nodes_ = node;
  }else
  {
    node->next(this->free_crontab_timers_);
    this->free_crontab_timers_ = node;
  }
}
timer_node *timer_queue::remove(int slot)
{
  STRACE("");

  timer_node *removed_node = this->timer_heap_[slot];

  --this->curr_size_;

  // Only try to reheapify if we're not deleting the last entry.
  if (slot < this->curr_size_)
  {
    timer_node *moved_node = this->timer_heap_[this->curr_size_];
    this->copy(slot, moved_node);

    // If the <moved_node->time_value_> is great than or equal its
    // parent it needs be moved down the heap.
    int parent = NDK_HEAP_PARENT(slot);
    if (moved_node->timer_value()
        >= this->timer_heap_[parent]->timer_value())
      this->reheap_down(moved_node,
                        slot,
                        NDK_HEAP_LCHILD(slot));
    else
      this->reheap_up(moved_node, slot, parent);
  }
  return removed_node;
}
int timer_queue::cancel(int timer_id,
                        const void **arg/* = 0*/,
                        int dont_call/* = 1*/)
{
  STRACE("");

  if (timer_id < 0 || timer_id >= this->timer_ids_size_)
    return -1;
  
  // 1. check crontab
  timer_node *iter = this->crontab_timers_;
  while (iter)
  {
    if (timer_id == iter->timer_id())
    {
      if (!dont_call)
      {
        event_handler *eh = iter->handler();
        if (eh) eh->handle_close(NDK_INVALID_HANDLE, event_handler::timer_mask);
      }
      if (arg != 0) *arg = iter->arg();
      if (this->crontab_timers_ != iter)
      {
        if (iter->prev() && iter->next())
        {
          iter->next()->prev(iter->prev());
          iter->prev()->next(iter->next());
        }else if (iter->prev() == 0)
        {
          iter->next()->prev(0);
          this->crontab_timers_ = iter->next();
        }else
        {
          iter->prev()->next(0);
          this->crontab_timers_ = iter->next();
        }
        iter->prev(0);
        iter->next(0);
      }
      this->free_node(iter);	
      return 0;
    }
    iter = iter->next();
  }

  // 2. general timer
  int timer_node_slot = this->timer_ids_[timer_id];
  // Check to see if timer_id is still valid.
  assert(timer_node_slot >= 0);
  if (timer_node_slot < 0)
    return -1;

  if (timer_id != this->timer_heap_[timer_node_slot]->timer_id())
  {
    assert(timer_id == this->timer_heap_[timer_node_slot]->timer_id());
    return -1;
  }else
  {
    timer_node *temp = this->remove(timer_node_slot);
    // Call the close hooks.
    if (!dont_call)
    {
      event_handler *eh = temp->handler();
      if (eh)
        eh->handle_close(NDK_INVALID_HANDLE, event_handler::timer_mask);
    }
    if (arg != 0)
      *arg = temp->arg();
    this->free_node(temp);	
  }
  return 0;
}
int timer_queue::cancel(const event_handler *eh,
                        int dont_call_handle_close/* = 1*/)
{
  STRACE("");
  if (eh == 0) return 0;
  int number_of_cancellations = 0;

  // 1. check crontab
  timer_node *iter = this->crontab_timers_;
  while (iter)
  {
    if (eh == iter->handler())
    {
      if (!dont_call)
      {
        event_handler *eh = iter->handler();
        if (eh) eh->handle_close(NDK_INVALID_HANDLE, event_handler::timer_mask);
      }
      timer_node *next_node = iter->next();
      if (this->crontab_timers_ != iter)
      {
        if (iter->prev() && iter->next())
        {
          iter->next()->prev(iter->prev());
          iter->prev()->next(iter->next());
        }else if (iter->prev() == 0)
        {
          iter->next()->prev(0);
          this->crontab_timers_ = iter->next();
        }else
        {
          iter->prev()->next(0);
          this->crontab_timers_ = iter->next();
        }
        iter->prev(0);
        iter->next(0);
      }
      number_of_cancellations++;
      this->free_node(iter);	
      iter = next_node;
      continue;
    }
    iter = iter->next();
  }

  // 2. general timer
  for (int i = 0; i < this->curr_size_; )
  {
    if (this->timer_heap_[i]->handler() == eh)
    {
      timer_node *temp = 0;
      temp = this->remove(i);
      number_of_cancellations++;
      this->free_node(temp);
      // We reset to zero so that we don't miss checking any nodes
      // if a reheapify occurs when a node is removed.  There
      // may be a better fix than this, however.
      i = 0;
    }else
      ++i;
  }
  if (!dont_call_handle_close)
  {
    event_handler *e = const_cast<event_handler *>(eh);
    e->handle_close(NDK_INVALID_HANDLE, event_handler::timer_mask);
  }
  return number_of_cancellations;
}
int timer_queue::reset_interval(int timer_id,
                                const time_value &interval)
{
  STRACE("");
  // Check to see if the timer_id is out of range
  if (timer_id < 0
      || timer_id > this->max_size_)
    return -1;
  int timer_node_slot = this->timer_ids_[timer_id];
  // Check to see if timer_id is still valid.
  if (timer_node_slot < 0) return -1;
  if (timer_id != this->timer_heap_[timer_node_slot]->timer_id())
  {
    assert(timer_id == this->timer_heap_[timer_node_slot]->timer_id());
    return -1;
  }else
  {
    // Reset the timer interval
    this->timer_heap_[timer_node_slot]->interval(interval);
  }
  return 0;
}
int timer_queue::reset_crontab(int timer_id,
                               const char *entry)
{
  STRACE("");
  // Check to see if the timer_id is out of range
  if (timer_id < 0
      || timer_id >= this->max_size_)
    return -1;
  timer_node *iter = this->crontab_timers_;
  while (iter)
  {
    if (timer_id == iter->timer_id())
    {
      timer_node *node = 0;
      node = this->alloc_node(CRONTAB_TIMER);
      if (node == 0) return -1;
      char **cron_entry = node->alloc_cron_entry();
      if (cron_entry == 0) return -1; 
      int result = this->parse_cron_entry(entry, cron_entry);
      if (result == -1)
      {
        this->free_node(node);
        return -1;
      }
      char **p = iter->crontab_entry();
      iter->crontab_entry(node->crontab_entry());
      node->crontab_entry(p);
      this->free_node(node);
      break;
    }
  }
  return 0;
}
int timer_queue::expire(const time_value &current_time)
{
  STRACE("");
  // Keep looping while there are timers remaining and the earliest
  // timer is <= the <cur_time> passed in to the method.
  if (this->is_empty())
    return 0;

  timer_node dispatched_node;
  if (this->dispatch_timer(current_time, dispatched_node))
  {
    if (this->upcall(dispatched_node.handler(),
                     &event_handler::handle_timeout,
                     dispatched_node.arg(),
                     current_time) < 0)
    {
      if (dispatched_node.interval() > time_value::zero)
      {
        this->cancel(dispatched_node.timer_id(), 0, 0);
      }
      // We must ensure the timer that expired one time, notify to user
      // when it call done
      else
      {
        dispatched_node.handler()->handle_close(NDK_INVALID_HANDLE,
                                                event_handler::timer_mask);
      }
    }
    return 1;
  }
  return 0;
}
int timer_queue::dispatch_timer(const time_value &current_time,
                                timer_node &dispatched_node)
{
  STRACE("");

  timer_node *expired = 0;
  if (this->earliest_time() <= current_time)
  {
    expired = this->remove_first();
    if (expired == 0) return 0;

    dispatched_node = *expired;

    if (expired->interval() > time_value::zero)
    {
      // Make sure that we skip past values that have already 
      // 'expired'.
      do
      {
        expired->timer_value(expired->timer_value() + 
                             expired->interval());
      }while (expired->timer_value() <= current_time);
      // Since this is an interval timer, we need to reschedule it.
      this->reschedule(expired);
    }else
    {
      this->free_node(expired);
    }
    return 1;
  }
  return 0;
}
timer_node *timer_queue::remove_first()
{
  if (this->curr_size_ == 0)
    return 0;
  return this->remove(0);
}
int timer_queue::upcall(event_handler *eh,
                        int(event_handler::*callback)(const void *act, 
                                                      const time_value &),
                        const void *arg,
                        const time_value &current_time)
{
  if (eh == 0) return -1;
  return(eh->*callback)(arg, current_time);
}
void timer_queue::verify_heap()
{
  for (int i = 0, parent = 0; 
       i < this->curr_size_; 
       ++i, parent = (i - 1) / 2)
  {
    assert((this->timer_heap_[i])->timer_value() 
           >= (this->timer_heap_[parent])->timer_value());
  }
}
} // namespace ndk
