// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 02:24
 */
//========================================================================

#ifndef NDK_INET_ADDR_H_
#define NDK_INET_ADDR_H_

#include "ndk/config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>          // for strtol
#include <cstring>
#include <netdb.h>

namespace ndk
{
  /**
   * @class inet_addr

   * @brief 
   */
  class inet_addr
  {
  public:
    inet_addr()
    {
      this->addr_type_ = AF_INET;
      std::memset(&this->inet_addr_, 0, sizeof(this->inet_addr_));
    }

    inline void *operator new (size_t bytes)
    { return ::new char[bytes]; }

    inline void operator delete (void *ptr)
    { ::delete [] ((char *)ptr); }

    // copy constructor
    explicit inet_addr(const inet_addr &addr)
    {
      std::memcpy(&this->inet_addr_, 
                  addr.get_addr(), 
                  sizeof(this->inet_addr_)); 
      this->set_type(addr.get_type());
    }

    // creates an inet_addr from a sockaddr_in structure.
    explicit inet_addr(const sockaddr_in *addr, int len)
    {
      this->reset();
      this->set(addr, len);
    }

    // 
    inet_addr(unsigned short port_number, const char *host_name)
    {
      this->reset();
      this->addr_type_ = AF_INET;
      this->set(port_number, host_name);
    }

    /** 
     * Create an inet_addr from the <address>, which can be 
     * "ip-number:port-number"(e.g., "tango.cs.wustl.edu:1234" or
     * "128.252.166.57:1234"). if there is no ':' in the <address> 
     * it is assumed to be a port number, with the IP address being 
     * INADDR_ANY.
     */
    explicit inet_addr(const char *address, int address_family = AF_INET)
    {
      this->reset();
      this->set(address, address_family);
    }

    /**
     * Creates an inet_addr from a <port_number> and an Internet 
     * <ip_addr>, This method assumes that <port_number> and <ip_addr>
     * are in host byte order. 
     */
    explicit inet_addr(unsigned short port_number, 
                       unsigned int ip_addr = INADDR_ANY)
    {
      this->reset();
      this->addr_type_ = AF_INET; 
      this->set(port_number, ip_addr);
    }

    inline inet_addr &operator =(const inet_addr &addr)
    {
      if (this != &addr)
      {
        std::memcpy(&this->inet_addr_, addr.get_addr(), sizeof(this->inet_addr_)); 
        this->addr_type_ = addr.addr_type_;
      }
      return *this;
    }

    // Creates an inet_addr from a sockaddr_in structure.
    inline int set(const sockaddr_in *addr, int len)
    {
      if (addr->sin_family == AF_INET)
      {
        int addr_size = static_cast<int>(sizeof(this->inet_addr_.in4_));
        if (len >= addr_size)
        {
          std::memcpy(&this->inet_addr_.in4_, addr, addr_size);
          this->addr_type_ = AF_INET;
          return 0;
        }
      }
      return -1;
    }

    /**
     * Initializes an inet_addr from the addr, which can be
     * "ip-number:port-number"(e.g., "tango.cs.wustl.edu:1234" or
     * "128.252.166.57:1234").  If there is no ':' in the <address> it
     * is assumed to be a port number, with the IP address being
     * INADDR_ANY.
     */
    inline int set(const char *addr, int address_family = AF_INET)
    { return this->string_to_addr(addr, address_family); }

    /**
     * Initializes an inet_addr from a port_number and an Internet ip_addr. 
     */
    inline int set(const unsigned short port_number, 
                   unsigned int inet_address = INADDR_ANY)
    {
      this->addr_type_ = AF_INET;
      this->inet_addr_.in4_.sin_family = AF_INET;
      this->inet_addr_.in4_.sin_port   = htons(port_number);
      unsigned int ip4 = htonl(inet_address);
      std::memcpy(&this->inet_addr_.in4_.sin_addr, &ip4, sizeof(unsigned int));
      return 0;
    }

    /**
     * Initializes an inet_addr from a <port_number> and the remote <host_name>.
     */
    int set(const unsigned short port_number, 
            const char *host_name, 
            int address_family = AF_INET);

    // 
    inline void set_type(int address_family)
    { this->addr_type_ = address_family; }

    // Equel
    inline bool operator ==(const inet_addr& addr) const
    {
      if (this->get_type() != addr.get_type()) 
        return false;
      return(std::memcmp(&this->inet_addr_, 
                         addr.get_addr(), 
                         sizeof(this->inet_addr_)) == 0);
    }

    // 
    inline bool operator !=(const inet_addr& addr) const
    { return !((*this) == addr); }

    /**
     * Transform the current inet_addr address into string format. 
     * If <ipaddr_format> is non-0 this produces "ip-number:port-number" 
     *(e.g., "128.252.166.57:1234"), whereas if <ipaddr_format> is 0 
     * this produces "ip-name:port-number"(e.g., "tango.cs.wustl.edu:1234"). 
     * Returns -1 if the size of the <buffer> is too small, else 0.
     */
    int addr_to_string(char *str, size_t size, int ipaddr_format = 1) const;

    /** 
     * Initializes an inet_addr from the @arg address, which can be
     * "ip-addr:port-number"(e.g., "128.252.166.57:1234") If there
     * is no ':' in the <address> it is assumed to be a port number,
     * with the IP address being INADDR_ANY.
     */
    int string_to_addr(const char *address, int address_family = AF_INET);

    /**
     * Return the "dotted decimal" Internet address representation 
     * of the hostname storing it in the addr(which is assumed to 
     * be <addr_size> bytes long). 
     */
    const char *get_host_addr(char *addr, size_t addr_size) const;

    /**
     * Return the character representation of the name of the host,
     * storing it in the <hostname>(which is assumed to be 
     * <hostname_len> bytes long). If <hostname_len> is greater 
     * than 0 then <hostname> will be NUL-terminated even if -2 is 
     * returned, error if -1.
     */
    int get_host_name(char *hostname, size_t hostname_len) const;

    // Get the port number, converting it into host byte-order.
    inline unsigned short get_port_number(void) const
    {
      if (this->get_type() == AF_INET)
        return ntohs(this->inet_addr_.in4_.sin_port);
      return 0;
    }

    // Return the 4-byte IP address, converting it into host byte
    // order.
    inline unsigned int get_ip_address(void) const
    { return ntohl(size_t(this->inet_addr_.in4_.sin_addr.s_addr)); }

    // 
    inline bool is_any(void) const
    { return (this->inet_addr_.in4_.sin_addr.s_addr == INADDR_ANY); }

    // Return true if the IP address is IPv4/ loopback address.
    inline bool is_loopback(void) const
    {
      // IPv4
      // RFC 3330 defines loopback as any address with 127.x.x.x
      return ((this->get_ip_address() & 0XFF000000) 
              == (INADDR_LOOPBACK & 0XFF000000));
    }
    inline bool is_multicast() const
    {
      // IPv4
      return this->inet_addr_.in4_.sin_addr.s_addr >= 0xE0000000 &&  // 224.0.0.0
        this->inet_addr_.in4_.sin_addr.s_addr <= 0xEFFFFFFF;   // 239.255.255.255
    }

    // 
    inline void* get_addr(void)
    { return static_cast<void*>(&this->inet_addr_); }

    //
    inline void* get_addr(void) const
    { return (void*)&this->inet_addr_; }

    //
    inline int get_type(void) const
    { return this->addr_type_; }

    //
    inline int get_addr_size(void) const
    { return static_cast<int>(sizeof(this->inet_addr_)); }

    // static member
    static const inet_addr addr_any;
  private:
    // Initialize underlying inet_addr_ to default values
    inline void reset(void)
    {
      std::memset(&this->inet_addr_, 0, sizeof (this->inet_addr_));
      if (this->get_type() == AF_INET)
      {
        this->inet_addr_.in4_.sin_family = AF_INET;
      }
    }
    // 
  private:
    // e.g., AF_UNIX, AF_INET, AF_SPIPE, etc.
    int  addr_type_;

    // Number of bytes in the address.
    //int  addr_size_;

    union
    {
      sockaddr_in	    in4_;
#if defined HAS_IPV6_
      sockaddr_in6    in6_;
#endif
    }inet_addr_;
  };
} // namespace ndk

#endif // NDK_INET_ADDR_H_

