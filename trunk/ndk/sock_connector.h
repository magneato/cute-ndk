// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 02:24
 */
//========================================================================

#ifndef NDK_SOCK_CONNECTOR_H_
#define NDK_SOCK_CONNECTOR_H_

#include "ndk/types.h"
#include "ndk/inet_addr.h"

namespace ndk
{
  // Forward declarations
  //
  class sock_stream;
  class time_value;

  /**
   * @class sock_connector
   *
   * @brief 
   */
  class sock_connector
  {
  public:
    /**
     * actively connect to a peer, construct a connected sock_stream 
     * object if the connection succeeds.
     *  
     * return  0: success
     * return -1: timeout or error
     */
    int connect(sock_stream &new_stream,
                const inet_addr &remote_addr,
                const time_value *timeout = 0,
                const inet_addr &local_addr = inet_addr::addr_any,
                int protocol_family = AF_INET,
                int reuse_addr = 0,
                size_t recvbuf_size = 0);
  protected:
    /** 
     * perform operations that ensure the socket is opened
     */
    int shared_open(sock_stream &new_stream, int protocol_family);

    int nb_connect(sock_stream &new_stream,
                   const inet_addr &remote_addr,
                   const time_value *timeout);
  };
} // namespace ndk

#endif // NDK_SOCK_CONNECTOR_H_

