#include <string.h>
#include <stdint.h>
#include <iomanip>
#include <sys/time.h>
#include <sys/select.h>
#include <list>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>

int g_include_lo = 0;
int g_colum_width = 14;
int g_sep_width = 5;

class net_info
{
public:
  net_info()
  : total_r_bytes(0),
  total_r_packages(0),
  total_t_bytes(0),
  total_t_packages(0)
  {  }

  std::string name;
  struct timeval check_time;
  int64_t total_r_bytes;
  int64_t total_r_packages;
  int64_t total_t_bytes;
  int64_t total_t_packages;
};
int parse(std::map<std::string, std::pair<net_info, net_info> > &eths,
          int first_one = 1)
{
  std::ifstream ifs("/proc/net/dev", std::ios::binary | std::ios::in);
  if (!ifs)
  {
    fprintf(stderr, "open `/proc/net/dev' failed!\n");
    return -1;
  }
  char line[1024] = {0};
  ifs.getline(line, 1023);  // title
  ifs.getline(line, 1023);  // title
  if (!g_include_lo)
    ifs.getline(line, 1023);  // loop back
  ::memset(line, 0, 1023);
  while (ifs.getline(line, 1023))
  {
    net_info eth;
    ::gettimeofday(&eth.check_time, 0);
    int64_t r_errors  = 0;
    int64_t r_drop    = 0;
    int64_t r_fifo    = 0;
    int64_t r_frame   = 0;
    int64_t r_compressed = 0;
    int64_t r_multicast = 0;

    eth.name.assign(line + 2, 4);
    sscanf(line + sizeof("  eth0:") - 1, 
           "%lld"  // total bytes
           "%lld"  // packets
           "%lld"  // errors
           "%lld"  // drop
           "%lld"  // fifo
           "%lld"  // frame
           "%lld"  // compressed
           "%lld"  // multicast

           "%lld"  // total bytes
           "%lld"  // packets
           ,
           &eth.total_r_bytes,
           &eth.total_r_packages,
           &r_errors,
           &r_drop,
           &r_fifo,
           &r_frame,
           &r_compressed,
           &r_multicast,
           &eth.total_t_bytes,
           &eth.total_t_packages
           );
    ::memset(line, 0, 1023);
    if (first_one)
      eths[eth.name].first = eth;
    else
      eths[eth.name].second = eth;
  }
  ifs.close();
  return 0;
}
void output(std::map<std::string, std::pair<net_info, net_info> > &eths)
{
  std::ostringstream  ostr;
  ostr << std::setfill(' ')
    << std::setiosflags(std::ios::left);
  ostr << std::setw(sizeof("interface")) << "interface";
  ostr << std::setw(g_colum_width) << "in-bytes";
  ostr << std::setw(g_colum_width) << "in-kbit/s";
  ostr << std::setw(g_colum_width) << "in-packages";
  ostr << std::setw(g_colum_width) << "in-p/s";

  ostr << std::setw(g_sep_width)   << "|";

  ostr << std::setw(g_colum_width) << "out-bytes";
  ostr << std::setw(g_colum_width) << "out-kbit/s";
  ostr << std::setw(g_colum_width) << "out-packages";
  ostr << std::setw(g_colum_width) << "out-p/s";
  ostr << std::endl;

  int64_t a_r_period_flux     = 0;
  int64_t a_r_period_packages = 0;
  int64_t a_r_bandwidth       = 0;
  int64_t a_r_p_bandwidth     = 0;

  int64_t a_t_period_flux     = 0;
  int64_t a_t_period_packages = 0;
  int64_t a_t_bandwidth       = 0;
  int64_t a_t_p_bandwidth     = 0;

  while (!eths.empty())
  {
    std::map<std::string, std::pair<net_info, net_info> >::iterator item;
    item = eths.begin();
    std::pair<net_info, net_info> eth = item->second;
    eths.erase(eth.first.name);

    if (eth.second.total_t_bytes == 0 && eth.second.total_r_bytes == 0)
      continue;

    int64_t sec = eth.second.check_time.tv_sec - eth.first.check_time.tv_sec;
    int64_t usec = eth.second.check_time.tv_usec - eth.first.check_time.tv_usec;
    if (usec < 0)
    {
      --sec;
      usec += 1000000;
    }
    int64_t msec = sec * 1000 + usec / 1000;

    // 
    int64_t r_bandwidth = 0;
    int64_t r_period_flux = eth.second.total_r_bytes - eth.first.total_r_bytes;
    if (r_period_flux < 0)
    {
      r_period_flux = int64_t(ULONG_MAX) + eth.second.total_r_bytes
        - eth.first.total_r_bytes;
    }
    if (r_period_flux > 0)
      r_bandwidth = (r_period_flux*1000/msec)/1024*8;

    //
    int64_t r_period_packages = eth.second.total_r_packages - eth.first.total_r_packages;
    int64_t r_p_bandwidth = 0;
    if (r_period_packages < 0)
      r_period_packages = int64_t(ULONG_MAX) + eth.second.total_r_packages
        - eth.first.total_r_packages;
    if (r_period_packages > 0)
      r_p_bandwidth = (r_period_packages*1000/msec);

    //
    int64_t t_period_flux = eth.second.total_t_bytes - eth.first.total_t_bytes;
    int64_t t_bandwidth = 0;
    if (t_period_flux < 0)
      t_period_flux = int64_t(ULONG_MAX) + eth.second.total_t_bytes
        - eth.first.total_t_bytes;
    if (t_period_flux > 0)
      t_bandwidth = (t_period_flux*1000/msec)/1024*8;

    //
    int64_t t_period_packages = eth.second.total_t_packages - eth.first.total_t_packages;
    int64_t t_p_bandwidth = 0;
    if (t_period_packages < 0)
      t_period_packages = int64_t(ULONG_MAX) + eth.second.total_t_packages
        - eth.first.total_t_packages;
    if (t_period_packages > 0)
      t_p_bandwidth = (t_period_packages*1000/msec);

    //
    a_r_period_flux += r_period_flux;
    a_r_period_packages += r_period_packages;
    a_r_bandwidth   += r_bandwidth;
    a_r_p_bandwidth += r_p_bandwidth;

    a_t_period_flux += t_period_flux;
    a_t_period_packages += t_period_packages;
    a_t_bandwidth   += t_bandwidth;
    a_t_p_bandwidth += t_p_bandwidth;

    ostr << std::setw(sizeof("interface")) << eth.first.name;
    ostr << std::setw(g_colum_width) << r_period_flux;
    ostr << std::setw(g_colum_width) << r_bandwidth;
    ostr << std::setw(g_colum_width) << r_period_packages;
    ostr << std::setw(g_colum_width) << r_p_bandwidth;

    ostr << std::setw(g_sep_width)             << " ";

    ostr << std::setw(g_colum_width) << t_period_flux;
    ostr << std::setw(g_colum_width) << t_bandwidth;
    ostr << std::setw(g_colum_width) << t_period_packages;
    ostr << std::setw(g_colum_width) << t_p_bandwidth;
    ostr << std::endl;
  }
  ostr << std::setw(sizeof("interface")) << "sum";
  ostr << std::setw(g_colum_width) << a_r_period_flux;
  ostr << std::setw(g_colum_width) << a_r_bandwidth;
  ostr << std::setw(g_colum_width) << a_r_period_packages;
  ostr << std::setw(g_colum_width) << a_r_p_bandwidth;

  ostr << std::setw(g_sep_width)             << " ";

  ostr << std::setw(g_colum_width) << a_t_period_flux;
  ostr << std::setw(g_colum_width) << a_t_bandwidth;
  ostr << std::setw(g_colum_width) << a_t_period_packages;
  ostr << std::setw(g_colum_width) << a_t_p_bandwidth;
  ostr << std::endl;

  ostr << std::endl;

  //
  std::cout << ostr.str();
}
void print_usage()
{
  printf("Usage: netview [OPTION...]\n\n");
  printf("  -i  seconds             Interval of every display net info.\n");
  printf("  -l                      Display loop back interface.\n");
  printf("\n");
}
int main (int argc, char *argv[])
{
  int interval = 3;

  const char *opt = ":i:lh";
  if (argc > 1)
  {
    int c = -1;
    extern int optind, optopt;
    while ((c = getopt(argc, argv, opt)) != -1)
    {
      switch(c)
      {
      case 'l':
        g_include_lo = 1;
        break;
      case 'i':
        interval = ::atoi(optarg);
        if (interval <= 0) 
          interval = 1;
        break;
      case ':':
        fprintf(stderr, "Option -%c requires an operand\n", optopt);
      case 'h':
      case '?':
      default:
        print_usage();
        return 0;
      }
    }
  }
  while (1)
  {
    std::map<std::string, std::pair<net_info, net_info> > eths;
    if (parse(eths) != 0)
      break;
    usleep(interval*1000*1000);
    if (parse(eths, 0) != 0)
      break;
    output(eths);
  }
  return 0;
}
