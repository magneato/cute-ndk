// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-11 23:52
 */
//========================================================================

#ifndef NDK_SOCKET_H_
#define NDK_SOCKET_H_

#include <fcntl.h>

#include "ndk/types.h"
#include "ndk/inet_addr.h"

namespace ndk
{
  /**
   * @class socket
   * 
   * @brief 
   */
  class socket
  {
  public:
    virtual ~socket() { }
    //
    inline int open(int type, int protocol_family)
    {
      ndk_handle h = ::socket(protocol_family, type, 0);
      if (h == NDK_INVALID_HANDLE) return -1;
      this->handle_ = h;
      return 0;
    }
    //
    inline int close(void)
    {
      if (this->handle_ != NDK_INVALID_HANDLE)
      {
        ::close(this->handle_);
        this->handle_ = NDK_INVALID_HANDLE;
      }
      return 0;
    }

    //
    inline int set_nonblock()
    {
      int flag = ::fcntl(this->get_handle(), F_GETFL, 0);
      if (flag == -1) return -1;
      if (flag & O_NONBLOCK) // already nonblock
        return 0;
      return ::fcntl(this->get_handle(), F_SETFL, flag | O_NONBLOCK);
    }

    //
    inline int set_block()
    {
      int flag = ::fcntl(this->get_handle(), F_GETFL, 0);
      if (flag == -1) return -1;
      if (flag & O_NONBLOCK) // already nonblock
        return ::fcntl(this->get_handle(), F_SETFL, flag & (~O_NONBLOCK));
      return 0;
    }

    //
    inline ndk_handle get_handle() const { return this->handle_; }
    inline void set_handle(ndk_handle h) { this->handle_ = h;    }

    //
    inline int get_local_addr(inet_addr &local_addr)
    {
      int l = local_addr.get_addr_size();
      struct sockaddr *addr = 
        reinterpret_cast<struct sockaddr*>(local_addr.get_addr());
      if (::getsockname(this->handle_, addr, (socklen_t *)&l) == -1)
        return -1;
      local_addr.set_type(addr->sa_family);
      return 0;
    }
    //
    inline int get_remote_addr(inet_addr &remote_addr)
    {
      int l = remote_addr.get_addr_size();
      struct sockaddr *addr = 
        reinterpret_cast<struct sockaddr*> (remote_addr.get_addr());
      if (::getpeername(this->handle_, addr, (socklen_t *)&l) == -1)
        return -1;
      remote_addr.set_type(addr->sa_family);
      return 0;
    }
    //
    static inline int reuseaddr(ndk_handle h, int val)
    {
      return ::setsockopt(h, 
                          SOL_SOCKET, 
                          SO_REUSEADDR, 
                          (const void*)&val,
                          sizeof(val));
    }
    //
    static inline int set_rcvbuf(ndk_handle h, size_t size)
    {
      if (size == 0) return -1;
      return ::setsockopt(h, 
                          SOL_SOCKET, 
                          SO_RCVBUF, 
                          (const void*)&size, 
                          sizeof(size));
    }
    //
    static inline int set_sndbuf(ndk_handle h, size_t size)
    {
      if (size == 0) return -1;
      return ::setsockopt(h, 
                          SOL_SOCKET, 
                          SO_SNDBUF, 
                          (const void*)&size, 
                          sizeof(size));
    }
    static inline int getsock_error(ndk_handle h, int &err)
    {
      socklen_t len = sizeof(int);
      return ::getsockopt(h,
                          SOL_SOCKET,
                          SO_ERROR,
                          &err,
                          &len);
    }
  protected:
    socket()
    : handle_(NDK_INVALID_HANDLE)
    { }

    ndk_handle handle_;
  };
} // namespace ndk

#endif // NDK_SOCKET_H_

