#include "ndk/date_time.h"

#include <string>
#include <cassert>

int main ()
{
  char str[32] = {0};
  printf("date is %s\n", ndk::date_time(0).to_str(str, sizeof(str)));
  printf("date is %s\n", ndk::date_time(ndk::date_time().date()).to_str(str, sizeof(str)));

  assert(ndk::date_time(ndk::date_time().str_to_datetime("2009-10-30 19:23:00")).to_str(str, sizeof(str)) 
         == std::string("2009-10-30 19:23:00"));
  assert(ndk::date_time(ndk::date_time().str_to_datetime("1970-10-30 19:23:00")).to_str(str, sizeof(str))
         == std::string("1970-10-30 19:23:00"));
  printf("date is %ld\n", ndk::date_time().str_to_datetime("1970-01-01 00:00:00"));

  assert(ndk::date_time(ndk::date_time().str_to_time("19:23:00")).to_str(str, sizeof(str))
         == std::string("1970-01-01 19:23:00"));
  assert(ndk::date_time(ndk::date_time().str_to_time("00:00:00")).to_str(str, sizeof(str))
         == std::string("1970-01-01 00:00:00"));
  assert(ndk::date_time(ndk::date_time().str_to_time("23:59:59")).to_str(str, sizeof(str))
         == std::string("1970-01-01 23:59:59"));

  assert(ndk::date_time(ndk::date_time().str_to_date("2009-10-30")).to_str(str, sizeof(str))
         == std::string("2009-10-30 00:00:00"));
  assert(ndk::date_time(ndk::date_time().str_to_date("1970-01-30")).to_str(str, sizeof(str))
         == std::string("1970-01-30 00:00:00"));

  assert(ndk::date_time(ndk::date_time().str_to_date("1970-01-30")).year() 
         == 1970);
  assert(ndk::date_time(ndk::date_time().str_to_date("1970-01-30")).month() 
         == 1);

  assert(ndk::date_time(ndk::date_time().str_to_date("1970-01-30")).mday() 
         == 30);

  ndk::date_time dt;
  dt.year(2008);
  assert(dt.year() == 2008);
  printf("date is %s\n", dt.to_str(str, sizeof(str)));
  dt.update();
  printf("date is %s\n", dt.to_str(str, sizeof(str)));

  return 0;
}
