// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 02:24
 */
//========================================================================

#ifndef NDK_SOCK_STREAM_H_
#define NDK_SOCK_STREAM_H_

#include "ndk/ndk.h"
#include "ndk/socket.h"
#include "ndk/time_value.h"

namespace ndk
{
  /**
   * @class sock_stream
   *
   * @brief 
   */
  class sock_stream : public socket
  {
  public:
    sock_stream() { }
    //
    sock_stream(ndk_handle h) { this->set_handle(h); } 
    //
    virtual ~sock_stream() { }

    //
  public:
    /**
     * peer closed connection return 0
     * time out or error return -1, buff is NULL 
     * actual recv bytes return > 0
     */
    int recv(void *buff, size_t len, 
             int flags, 
             const time_value *timeout = 0)
    { return ndk::recv(this->get_handle(), buff, len, flags, timeout); }

    /**
     * peer closed connection return 0
     * time out or error return -1(may be buff has data but not completely)
     * actual recv bytes return > 0
     */
    int recv_n(void *buff, size_t len, 
               int flags, 
               const time_value *timeout = 0)
    { return ndk::recv_n(this->get_handle(), buff, len, flags, timeout); }

    //
    int recv  (void *buff, size_t len, const time_value *timeout = 0)
    { return ndk::recv(this->get_handle(), buff, len, timeout); }
    int recv_n(void *buff, size_t len, const time_value *timeout = 0)
    { return ndk::recv_n(this->get_handle(), buff, len, timeout); }

    // 
    int recvv  (iovec iov[], size_t count, const time_value *timeout = 0)
    { return ndk::recvv(this->get_handle(), iov, count, timeout); }
    int recvv_n(iovec iov[], size_t count, const time_value *timeout = 0)
    { return ndk::recvv_n(this->get_handle(), iov, count, timeout); }

    //
    int send  (const void *buff, size_t len, 
               int flags, 
               const time_value *timeout = 0)
    { return ndk::send(this->get_handle(), buff, len, flags, timeout); }
    int send_n(const void *buff, size_t len, 
               int flags, 
               const time_value *timeout = 0)
    { return ndk::send_n(this->get_handle(), buff, len, flags, timeout); }

    //
    int send  (const void *buff, size_t len, const time_value *timeout = 0)
    { return ndk::send(this->get_handle(), buff, len, timeout); }
    int send_n(const void *buff, size_t len, const time_value *timeout = 0)
    { return ndk::send_n(this->get_handle(), buff, len, timeout); }

    //
    int sendv  (const iovec iov[], size_t count, const time_value *timeout = 0)
    { return ndk::sendv(this->get_handle(), iov, count, timeout); }
    int sendv_n(const iovec iov[], size_t count, const time_value *timeout = 0)
    { return ndk::sendv_n(this->get_handle(), iov, count, timeout); }
  };
} // namespace ndk

#endif // NDK_SOCK_STREAM_H_

