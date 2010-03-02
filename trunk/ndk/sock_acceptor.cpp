#include "ndk/sock_acceptor.h"
#include "ndk/sock_stream.h"
#include "ndk/strace.h"

#include <sys/errno.h>

namespace ndk
{
int sock_acceptor::open(const inet_addr &local_addr,
                        size_t rcvbuf_size/* = 0*/,
                        int reuse_addr/* = 1*/,
                        int protocol_family/* = AF_INET*/,
                        int backlog/* = NDK_DEFAULT_BACKLOG*/)
{
  STRACE("sock_acceptor");
  if (socket::open(SOCK_STREAM, protocol_family) == -1)
    return -1;

  int error = 0;
  if (rcvbuf_size != 0)
    socket::set_rcvbuf(this->get_handle(), rcvbuf_size);
  if (reuse_addr)
    socket::reuseaddr(this->get_handle(), 1);
  // create ok
  if (protocol_family == AF_INET)
  {
    sockaddr_in local_inet_addr;
    std::memset(reinterpret_cast<void *>(&local_inet_addr),
                0,
                sizeof(local_inet_addr));
    if (local_addr == inet_addr::addr_any)
    {
      local_inet_addr.sin_port  = 0;
    }else
    {
      local_inet_addr = *reinterpret_cast<sockaddr_in *>(local_addr.get_addr());
    }
    if (local_inet_addr.sin_port == 0)
    {
      if (ndk::bind_port(this->get_handle(), 
                         ntohl(local_inet_addr.sin_addr.s_addr), 
                         protocol_family) == -1)
      {
        error = 1;
      }
    }else
      if (::bind(this->get_handle(),
                 reinterpret_cast<sockaddr *>(&local_inet_addr),
                 sizeof(local_inet_addr)) == -1)
      {
        error = 1;
      }
  }else error = 1;   // no support
  if (error != 0 || ::listen(this->get_handle(), backlog) == -1)
  {
    this->close(); return -1;
  }
  return 0;
}
int sock_acceptor::accept(sock_stream &new_stream,
                          inet_addr *remote_addr/* = 0*/,
                          const time_value *timeout/* = 0*/)
{
  STRACE("sock_acceptor");
  int block_mode   = 0;
  socklen_t *len_ptr = 0;
  sockaddr *addr   = 0;
  int len          = 0;
  if (remote_addr != 0)
  {
    len     = remote_addr->get_addr_size();
    len_ptr = (socklen_t *)&len;
    addr    = (sockaddr *)remote_addr->get_addr();
  }
  if (timeout != 0)
  {
    ndk::record_and_set_non_block_mode(this->get_handle(), 
                                       block_mode);  // 1. set
    if (*timeout > time_value::zero)
    {
      if (ndk::handle_read_ready(this->get_handle(), 
                                 timeout) == -1)  // timeout or error
        return -1;
    }
    new_stream.set_handle(::accept(this->get_handle(), 
                                   addr, 
                                   len_ptr));
    ndk::restore_non_block_mode(this->get_handle(), 
                                block_mode);  // 2. restore
  }else
  {
    do
    {
      new_stream.set_handle(::accept(this->get_handle(), 
                                     addr, 
                                     len_ptr));
    }while (new_stream.get_handle() == NDK_INVALID_HANDLE
            && errno == EINTR);
  }
  if (new_stream.get_handle() != NDK_INVALID_HANDLE
      && remote_addr != 0)
  {
    if (addr)
      remote_addr->set_type(addr->sa_family);
  }
  return new_stream.get_handle() != NDK_INVALID_HANDLE ? 0 : -1;
}
} // namespace ndk
