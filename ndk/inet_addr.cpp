#include "ndk/inet_addr.h"
#include "ndk/config.h"

#include <cstdio>

namespace ndk
{
const inet_addr inet_addr::addr_any;

int inet_addr::addr_to_string(char *str, 
                              size_t size, 
                              int ipaddr_format/* = 1*/) const
{
  char hoststr[MAX_HOSTNAME_LEN+1] = {0};
  if (str == NULL || size <= 0) return -1;
  bool result = false;
  if (ipaddr_format == 0)      
  {
    result =(this->get_host_name(hoststr, MAX_HOSTNAME_LEN) != 0);
  }else  // default
  {
    result =(this->get_host_addr(hoststr, MAX_HOSTNAME_LEN) != 0);
  }
  size_t total_len = ::strlen(hoststr)
    + 5  // strlen("65535")
    + 1  // sizeof ":"
    + 1; // sizeof '\0' 
  if (size < total_len) return -1;
  ::sprintf(str, "%s:%d", 
            hoststr,
            this->get_port_number());
  return 0;
}
int inet_addr::string_to_addr(const char *address, int address_family/* = AF_INET*/)
{
  int ret_val   = -1;
  char ip_addr[32] = {0};  
  ::strncpy(ip_addr, address, 32);
  char *port_p  = ::strrchr(ip_addr, ':');
  if (port_p == 0)    // Assume it's a port number.
  {
    unsigned short port_num = ::atoi(ip_addr);
    if (port_num == 0)
      return -1;
    ret_val = this->set(port_num, static_cast<unsigned int>(INADDR_ANY));
  }else
  {
    *port_p = '\0'; ++port_p; // skip over ':'
    unsigned short port_num = ::atoi(port_p);
    ret_val = this->set(port_num, ip_addr, address_family);
  }
  return ret_val;
}
const char *inet_addr::get_host_addr(char *addr, size_t addr_size) const
{
  // AF_INET4
  char *ch = ::inet_ntoa(this->inet_addr_.in4_.sin_addr);
  ::strncpy(addr, ch, addr_size);
  return ch;
}
int inet_addr::get_host_name(char *hostname, size_t len) const
{
  if (this->inet_addr_.in4_.sin_addr.s_addr == INADDR_ANY)
    {
      return ::gethostname(hostname, len);
    }
  struct hostent *host = 0;
  host = ::gethostbyaddr(reinterpret_cast<const char*>(&(this->inet_addr_.in4_.sin_addr)), 
                         sizeof(struct in_addr), 
                         AF_INET);
  if (host != 0 && hostname != 0)
    {
      if (::strlen(host->h_name) >= len)
        {
          if (len > 0)
            {
              ::memcpy(hostname, host->h_name, len - 1);
              hostname[len-1] = '\0';
            }
          return -2;
        }
      ::strncpy(hostname, host->h_name, len);
      return 0;
    }
  return -1;
}
int inet_addr::set(const unsigned short port_number, 
                   const char *host_name,
                   int address_family/* = AF_INET*/)
{
  // IPv4
  if (host_name == 0)
    return -1;
  std::memset((void *)&this->inet_addr_, 0, sizeof(this->inet_addr_));
  address_family = AF_INET;
  this->addr_type_ = address_family;
  this->inet_addr_.in4_.sin_family = static_cast<short>(address_family);
  struct in_addr addrv4;
  if (::inet_aton(host_name, &addrv4) != 0)
    {
      return this->set(port_number, ntohl(addrv4.s_addr));
    }else
    {
      struct hostent *host = 0;
      host = ::gethostbyname(host_name);
      if (host != 0)
        {
          std::memcpy((void *)&addrv4.s_addr, host->h_addr, host->h_length);
          return this->set(port_number, ntohl(addrv4.s_addr));
        }
    }
  return -1;
}
} // namespace ndk
