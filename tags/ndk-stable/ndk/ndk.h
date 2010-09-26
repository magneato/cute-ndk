// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 02:24
 */
//========================================================================

#ifndef NDK_NDK_H_
#define NDK_NDK_H_

#include <fcntl.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/socket.h>

#include "ndk/types.h"
#include "ndk/time_value.h"
#include "ndk/global_macros.h"

namespace ndk
{
  // Forward declarations.
  //

  // = system config
  extern int max_handles();

  extern int set_handle_limit(int new_limit,
                              int increase_limit_only = 0);

  // = process
  extern int daemon(bool close_all_handles = true, const char *work_path = 0);

  // = select opt
  extern int handle_ready(ndk_handle handle, 
                          int r_ready,
                          int w_ready,
                          int e_ready,
                          const time_value *timeout = 0);

  extern inline int handle_read_ready(ndk_handle handle,
                                      const time_value *timeout = 0)
  { return ndk::handle_ready(handle, 1, 0, 0, timeout); }

  extern inline int handle_write_ready(ndk_handle handle,
                                       const time_value *timeout = 0)
  { return ndk::handle_ready(handle, 0, 1, 0, timeout); }

  extern inline int handle_exception_ready(ndk_handle handle,
                                           const time_value *timeout = 0)
  { return ndk::handle_ready(handle, 0, 0, 1, timeout); }

  // = handle opt
  extern inline int set_non_block_mode(ndk_handle handle)
  {
    int flag = ::fcntl(handle, F_GETFL, 0);
    if (NDK_BIT_ENABLED(flag, O_NONBLOCK)) // already nonblock
      return 0;
    return ::fcntl(handle, F_SETFL, flag | O_NONBLOCK);
  }

  extern inline int set_block_mode(ndk_handle handle)
  {
    int flag = ::fcntl(handle, F_GETFL, 0);
    if (NDK_BIT_ENABLED(flag, O_NONBLOCK)) // nonblock
    {
      NDK_CLR_BITS(flag, O_NONBLOCK);
      return ::fcntl(handle, F_SETFL, flag);
    }
    return 0; 
  }

  extern inline int record_and_set_non_block_mode(ndk_handle handle,
                                                  int &flag)
  {
    flag = ::fcntl(handle, F_GETFL, 0);
    if (NDK_BIT_ENABLED(flag, O_NONBLOCK)) // already nonblock
      return 0;
    return ::fcntl(handle, F_SETFL, flag | O_NONBLOCK);
  }

  extern inline int restore_non_block_mode(ndk_handle handle,
                                           int flag)
  {
    if (NDK_BIT_ENABLED(flag, O_NONBLOCK)) // nonblock original
      return 0;
    // block original and clear it
    //int val = flag;
    //val &= ~O_NONBLOCK;
    return ::fcntl(handle, F_SETFL, (flag & (~O_NONBLOCK)));
  }
  // = sleep
  extern inline void sleep(const time_value &tv)
  {
    ndk::handle_ready(0, 0, 0, 0, &tv);
  }

  // = socket io
  extern int recv(ndk_handle handle, 
                  void *buff, 
                  size_t len, 
                  int flags, 
                  const time_value *timeout);

  extern inline int recv(ndk_handle handle, 
                         void *buff, 
                         size_t len, 
                         const time_value *timeout)
  { return ndk::recv(handle, buff, len, 0, timeout); }

  extern int recv_n(ndk_handle handle, 
                    void *buff, 
                    size_t len, 
                    int flags, 
                    const time_value *timeout);

  extern inline int recv_n(ndk_handle handle, 
                           void *buff, 
                           size_t len, 
                           const time_value *timeout)
  { return ndk::recv_n(handle, buff, len, 0, timeout); }

  extern int recvv(ndk_handle handle, 
                   iovec iov[], 
                   size_t count, 
                   const time_value *timeout);

  extern int recvv_n(ndk_handle handle, 
                     iovec iov[], 
                     size_t count, 
                     const time_value *timeout);

  extern int recvfrom(ndk_handle handle, 
                      void *buff, 
                      size_t len, 
                      int flags,
                      struct sockaddr *from, 
                      int *fromlen,
                      const time_value *timeout);

  extern inline int recvfrom(ndk_handle handle, 
                             void *buff, 
                             size_t len, 
                             struct sockaddr *from, 
                             int *fromlen,
                             const time_value *timeout)
  { return ndk::recvfrom(handle, buff, len, 0, from, fromlen, timeout); }

  // == 
  extern int send(ndk_handle handle, 
                  const void *buff, 
                  size_t len, 
                  int flags, 
                  const time_value *timeout);

  extern inline int send(ndk_handle handle, 
                         const void *buff, 
                         size_t len, 
                         const time_value *timeout)
  { return ndk::send(handle, buff, len, 0, timeout); }

  extern int send_n(ndk_handle handle, 
                    const void *buff,
                    size_t len, 
                    int flags, 
                    const time_value *timeout);

  extern inline int send_n(ndk_handle handle, 
                           const void *buff, 
                           size_t len, 
                           const time_value *timeout)
  { return ndk::send_n(handle, buff, len, timeout); }

  extern int sendv(ndk_handle handle, 
                   const iovec iov[], 
                   size_t count, 
                   const time_value *timeout);

  extern int sendv_n(ndk_handle handle, 
                     const iovec iov[], 
                     size_t count, 
                     const time_value *timeout);

  extern int sendto(ndk_handle handle, 
                    const void *buff, 
                    size_t len, 
                    int flags, 
                    const struct sockaddr *to,
                    int tolen,
                    const time_value *timeout);

  extern int sendto(ndk_handle handle, 
                    const void *buff, 
                    size_t len, 
                    const struct sockaddr *to,
                    int tolen,
                    const time_value *timeout);

  extern int read(ndk_handle handle, 
                  void *buff, 
                  size_t len); 

  extern int read_n(ndk_handle handle, 
                    void *buff, 
                    size_t len);

  extern int write(ndk_handle handle, 
                   const void *buff, 
                   size_t len); 

  extern int write_n(ndk_handle handle, 
                     const void *buff, 
                     size_t len); 

  // Bind socket to an unused port.
  extern int bind_port(ndk_handle handle,
                       size_t ip_addr, 
                       int addr_family);

  // = System info
  extern int get_cpu_count(void);

  // = String operation.
  extern char *strtok(char *str, 
                      const char *sep, 
                      char **last);

  // Remove blanks at the beginning and the end of a string.
  extern char *strstrip(const char *in_str, 
                        char *out_str, 
                        int out_str_len);

  // Remove blanks all of a string.
  extern char *strstrip_all(const char *in_str, 
                            char *out_str, 
                            int out_str_len);

  extern thread_func_return_t thr_run_reactor_event_loop(void *arg);
} // namespace ndk
#endif // NDK_NDK_H_

