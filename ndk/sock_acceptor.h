// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 02:24
 */
//========================================================================

#ifndef NDK_SOCK_ACCEPTOR_H_
#define NDK_SOCK_ACCEPTOR_H_

#include "ndk/socket.h"

namespace ndk
{
  /**
   * @class sock_acceptor
   *
   * @brief 
   */
  class sock_stream;
  class time_value;
  class sock_acceptor : public socket
  {
  public:
    sock_acceptor()
    { } 

    virtual ~sock_acceptor()
    { }

    /**
     * Initialize a passive-mode BSD-style acceptor socket
     * <local_addr> is the address that we're going to listen for
     * connections on.  If <reuse_addr> is 1 then we'll use the
     * <SO_REUSEADDR> to reuse this address.  Returns 0 on success and
     * -1 on failure.
     */
    int open(const inet_addr &local_addr,
             size_t revbuf_size = 0,
             int reuse_addr = 1,
             int protocol_family = AF_INET,
             int backlog = NDK_DEFAULT_BACKLOG);

    /**
     * Accept a new sock_stream connection.  timeout_msec is 0
     * means non-block, = -1 means block forever, > 0 means timeout
     */
    int accept(sock_stream &new_stream,
               inet_addr *remote_addr = 0,
               const time_value *timeout = 0);

  };
} // namespace ndk

#endif // NDK_SOCK_ACCEPTOR_H_

