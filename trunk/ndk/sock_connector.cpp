#include "ndk/sock_connector.h"
#include "ndk/sock_stream.h"
#include "ndk/strace.h"

#include <sys/errno.h>

namespace ndk
{
int sock_connector::shared_open(sock_stream &new_stream, 
                                int protocol_family)
{
  STRACE("");
  if (new_stream.get_handle() == NDK_INVALID_HANDLE
      && new_stream.open(SOCK_STREAM, protocol_family) == -1)
    return -1;
  return 0;
}
int sock_connector::connect(sock_stream &new_stream,
                            const inet_addr &remote_addr,
                            const time_value *timeout/* = 0*/,
                            const inet_addr &local_addr/* = inet_addr::addr_any*/,
                            int protocol_family/* = AF_INET*/,
                            int reuse_addr/* = 0*/,
                            size_t recvbuf_size/* = 0*/)
{
  STRACE("sock_connector");
  if (this->shared_open(new_stream, protocol_family) == -1)
    return -1;
  if (local_addr != inet_addr::addr_any)
  {
    sockaddr *laddr = reinterpret_cast<sockaddr *>(local_addr.get_addr());
    int size = local_addr.get_addr_size();
    if (::bind(new_stream.get_handle(), laddr, size) == -1)
    {
      new_stream.close();
      return -1;
    }
  }

  if (recvbuf_size != 0 &&
      socket::set_rcvbuf(new_stream.get_handle(), recvbuf_size) == -1)
  {
    new_stream.close();
    return -1;
  }
  if (reuse_addr)
    socket::reuseaddr(new_stream.get_handle(), 1);

  // 
  if (timeout == 0)
  {
    return ::connect(new_stream.get_handle(), 
                     reinterpret_cast<sockaddr *>(remote_addr.get_addr()),
                     remote_addr.get_addr_size());
  }
  // timeout use nonblock connect
  return this->nb_connect(new_stream, remote_addr, timeout);
}
int sock_connector::nb_connect(sock_stream &new_stream,
                               const inet_addr &remote_addr,
                               const time_value *timeout)
{
  STRACE("sock_connector");
  new_stream.set_nonblock();
  int ret_val = ::connect(new_stream.get_handle(),
                          reinterpret_cast<sockaddr *>(remote_addr.get_addr()),
                          remote_addr.get_addr_size());
  int err_num = errno;
  if (ret_val == -1)
  {
    if (err_num == EINPROGRESS)
    {
      if (*timeout != time_value::zero)
      {
        ret_val = ndk::handle_ready(new_stream.get_handle(), 
                                    1, 
                                    1, 
                                    0, 
                                    timeout);
        if (ret_val == 0) // timeout
        {
          ret_val = -1; // error
          err_num = ETIMEDOUT;
        }else if (ret_val > 0)
        {
          //socklen_t len = sizeof(ret_val);
          int sockopt_ret = socket::getsock_error(new_stream.get_handle(), 
                                                  ret_val);
          if (ret_val != 0)      // berkeley style
          {
            err_num = ret_val;
            ret_val = -1;
          }
          if (sockopt_ret < 0)   // solaris style
          {
            err_num = errno;
            ret_val = -1;
          }
        }// failed
      }
    }
  }

  new_stream.set_block();
  if (ret_val == -1 && err_num != EINPROGRESS) 
    new_stream.close();
  if (ret_val == -1 && err_num != 0) 
    errno = err_num;
  return ret_val;
}
} // namespace ndk
