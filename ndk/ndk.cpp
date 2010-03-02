#include "ndk/ndk.h"
#include "ndk/thread.h"
#include "ndk/inet_addr.h"
#include "ndk/time_value.h"
#include "ndk/unused_arg.h"
#include "ndk/reactor_impl.h"

#if defined NDK_LINUX
#  include <errno.h>                       
#  include <ctype.h>
#  include <malloc.h>                                     
#  include <cstring>
#  include <sys/select.h>
#  include <sys/resource.h>                          
#elif defined NDK_WIN32   

#endif

#include <cstdio>
#include <cassert>

int ndk::max_handles()
{      
#if defined NDK_LINUX
  struct rlimit rl;                       
  std::memset((void *)&rl, 0, sizeof(rl));             
  int r = ::getrlimit(RLIMIT_NOFILE, &rl);
# if !defined(RLIM_INFINITY)         
  if (r == 0) return rl.rlim_cur;
# else     
  if (r == 0 && rl.rlim_cur != RLIM_INFINITY)
    return rl.rlim_cur;
# endif                                                            
#endif
  return -1;                          
}   
int ndk::set_handle_limit(int new_limit,
                          int increase_limit_only/* = 0*/)                
{
#if defined(NDK_LINUX)
  int cur_limit = ndk::max_handles();
  if (cur_limit == -1) return -1;
  struct rlimit rl;
  std::memset((void *)&rl, 0, sizeof(rl));
  int r = ::getrlimit(RLIMIT_NOFILE, &rl);
  if (r < 0) return -1;
  if (new_limit < 0) return -1;

  if (new_limit > cur_limit)
  {
    rl.rlim_cur = new_limit;
    return ::setrlimit(RLIMIT_NOFILE, &rl);
  }
  else if (increase_limit_only == 0)
  {
    rl.rlim_cur = new_limit;
    return ::setrlimit(RLIMIT_NOFILE, &rl);
  }
#endif
  return 0;
}
int ndk::daemon(bool close_all_handles/* = true*/, 
                const char *work_path/* = 0*/)
{
#if defined NDK_LINUX
  int result = 0;
  pid_t pid = fork();
  switch(pid)
  {
  case -1:
    fprintf(stderr, "fork failed! [%s]", strerror(errno));
    return -1;
  case 0:   break;
  default:  ::exit(0);
  }
  // child process
  setsid();
  pid = fork();
  //
  if (pid != 0) exit(0);
  // change working directory.
  if (work_path) 
    result = ::chdir(work_path);
  if (result != 0) return -1;

  umask(0);
  if (close_all_handles)
  {
    for (int i = ndk::max_handles() - 1; i >= 0; i--)
      ::close(i);
    int fd = ::open("/dev/null", O_RDWR, 0);
    if (fd != -1)
    {
      dup2(fd, STDIN_FILENO);
      dup2(fd, STDOUT_FILENO);
      dup2(fd, STDERR_FILENO);
      if (fd > STDERR_FILENO)
        ::close(fd);
    }
  }
#endif
  return 0;
}
// --------------------------------------------------------------------
int ndk::handle_ready(ndk_handle handle,
                      int r_ready,
                      int w_ready,
                      int e_ready,
                      const time_value* timeout)
{
#if defined NDK_LINUX
  int width = handle + 1; //
  assert(handle < FD_SETSIZE);
#elif defined NDK_WIN32
  int width = 0;
#endif
  fd_set rset, wset, eset;

  // must define here and not define in while loop
  struct timeval tv;
  if (timeout != 0)
  {
    tv = *timeout;
  }
  int ret_val = 0;
  while (1)
  {
    FD_ZERO(&rset);
    FD_SET(handle, &rset);
    ret_val = ::select(width,   r_ready ? &rset : 0,
                       w_ready ? &(wset = rset) : 0,
                       e_ready ? &(eset = rset) : 0,
                       timeout == 0 ? 0 : &tv);
    // not clear the fd_set return -1, POSIX 1.0v: timeval would be modify
    // clear all the fd_set return 0(timeout)
    if ((ret_val == -1) &&(errno == EINTR))// interupt by signal
      continue;
    break;  // timeout or ready
  }
  return ret_val;
}
int ndk::recv(ndk_handle handle,
              void *buff,
              size_t len,
              int flags,
              const time_value *timeout)
{
  int ret = 0;
  // complete block
  if (timeout == 0)  // block mode, handle by tcp stack
  {
    do{
      ret = ::recv(handle, buff, len, flags);
    }while (ret == -1 && errno == EINTR);
    return ret;
  }

  // set nonblock mode
  int flag = 0;
  ndk::record_and_set_non_block_mode(handle, flag);

  // try it first
  ret = ::recv(handle, buff, len, flags);
  if (ret >= 0 // recv successfuly
      || (ret == -1 && errno != EWOULDBLOCK && errno != EAGAIN))
    // socket error
    return ret;
  // timeout
  if (*timeout > time_value::zero)
  {
    ret = ndk::handle_read_ready(handle, timeout);
    if (ret > 0)
    {
      ret = ::recv(handle, buff, len, flags);
    }else  // timeout or error
      ret = -1;
  }
  // reset to block mode
  ndk::restore_non_block_mode(handle, flag);
  return ret; // return -1 if no data in tcp stack buff under non-block mode
}
int ndk::recv_n(ndk_handle handle, 
                void *buff, 
                size_t len, 
                int flags, 
                const time_value *timeout) 
{
  size_t recv_bytes = 0;
  int error = 0;
  int result = 0;
  // set nonblock
  int flag = 0;
  if (timeout != 0)
    ndk::record_and_set_non_block_mode(handle, flag);
  //
  for (int n = 0; recv_bytes < len; recv_bytes += n)
  {
    n = ::recv(handle, 
               static_cast<char *>(buff) + recv_bytes, 
               len - recv_bytes, 
               flags);
    if (n <= 0)  // peer closed or error
    {
      // check for possible blocking.
      if (n == -1 && (errno == EWOULDBLOCK || errno == EAGAIN))
      {
        if (timeout == 0 || (timeout != 0 && *timeout > time_value::zero))
        {
          if (ndk::handle_read_ready(handle, timeout) > 0) 
          {
            n = 0;  // !!
            continue;
          }
        }
      }else if (n == -1 && errno == EINTR)  // normal error
      {
        n = 0;  // !!
        continue;  
      }else
        error = 1;
      // if select() timeout, should return the bytes recv actually
      result = n;
      break;
    } // if (n <= 0)
  }
  if (error)
    return result;
  // reset to block mode
  if (timeout != 0)
    ndk::restore_non_block_mode(handle, flag);
  return recv_bytes;
}
int ndk::recvv(ndk_handle handle, 
               iovec iov[], 
               size_t count, 
               const time_value *timeout) 
{
  int ret = 0;
  if (timeout == 0)
  {
    do{
      ret = ::readv(handle, iov, count);
    }while (ret == -1 && errno == EINTR);

    return ret;
  }

  // set nonblock mode
  int flag = 0;
  ndk::record_and_set_non_block_mode(handle, flag);
  // try it first
  ret = ::readv(handle, iov, count);
  if (ret >= 0 // recv successfuly
      ||(ret == -1 && errno != EWOULDBLOCK && errno != EAGAIN)) 
    // socket error
    return ret;
  // timeout
  if (*timeout > time_value::zero)
  {
    ret = ndk::handle_read_ready(handle, timeout);
    if (ret	> 0) 
    {
      ret = ::readv(handle, iov, count);
    }else
      ret = -1;
  }
  // reset to block mode
  ndk::restore_non_block_mode(handle, flag);
  return ret;
}
int ndk::recvv_n(ndk_handle handle, 
                 iovec iv[], 
                 size_t count, 
                 const time_value *timeout) 
{
  ssize_t n = 0;
  size_t recv_bytes = 0;
  int error = 0;
  int result = 0;
#if IOVEC_SECURITY_IO
  // for bak
  struct iovec *iov = iv;
  struct iovec *iov_bak = 
    (struct iovec*)::malloc(sizeof(struct iovec) * count);
  for (size_t i = 0; i < count; ++i)
    iov_bak[i] = iov[i]; 
  bool bfull = true;
#else
  struct iovec *iov = iv;
#endif
  // set nonblock
  int flag = 0;
  if (timeout != 0)
    ndk::record_and_set_non_block_mode(handle, flag);
  for (size_t s = 0; s < count;)
  {
    n = ::readv(handle, iov + s, count - s);
    if (n == 0 || // peer closed
        n == -1)  // check for errors
    {
      // check for possible blocking.
      if (n == -1 && 
          (errno == EAGAIN || errno == EWOULDBLOCK))
      {
        if (timeout == 0 ||(timeout != 0 && *timeout > time_value::zero))
        {
          if (ndk::handle_read_ready(handle, timeout) > 0) 
          {
            n = 0;  // !!
            continue;
          }
        }
      }else if (n == -1 && errno == EINTR) 
      {
        n = 0;  // !!
        continue;  
      }else
        error = 1;
      result = n;
      break;
    }
    for (recv_bytes += n; 
         s < count && n >= static_cast<int>(iov[s].iov_len); 
         ++s)
      n -= iov[s].iov_len;
#if IOVEC_SECURITY_IO
    if (n != 0)
    {
      bfull = false;
      char *base = static_cast<char *>(iov[s].iov_base);
      iov[s].iov_base = base + n;
      iov[s].iov_len  = iov[s].iov_len - n;
    }
#endif
  }
#if IOVEC_SECURITY_IO
  if (!bfull)
  {
    for (size_t i = 0; i < count; ++i)
      iov[i] = iov_bak[i]; 
  }
  ::free(iov_bak);
#endif
  // reset to block mode
  if (timeout != 0)
    ndk::restore_non_block_mode(handle, flag);
  if (error)
    return result;
  return recv_bytes;
}
int ndk::recvfrom(ndk_handle handle, 
                  void *buff, 
                  size_t len, 
                  int flags, 
                  struct sockaddr *from, 
                  int *fromlen,
                  const time_value *timeout) 
{
  int ret = 0;
  // complete block
  if (timeout == 0)  // block mode, handle by tcp stack
  {
    do{
      ret = ::recvfrom(handle, buff, len, flags, from, (socklen_t *)fromlen);
    }while (ret == -1 && errno == EINTR);
    return ret;
  }

  // set nonblock mode
  int flag = 0;
  ndk::record_and_set_non_block_mode(handle, flag);

  // try it first
  ret = ::recvfrom(handle, buff, len, flags, from, (socklen_t *)fromlen);
  if (ret >= 0 // recv successfuly
      || (ret == -1 && errno != EWOULDBLOCK && errno != EAGAIN)) 
    // socket error
    return ret;
  // timeout
  if (*timeout > time_value::zero)
  {
    ret = ndk::handle_read_ready(handle, timeout);
    if (ret > 0) 
    {
      ret = ::recvfrom(handle, buff, len, flags, from, (socklen_t *)fromlen);
    }else  // timeout or error
      ret = -1;
  }
  // reset to block mode
  ndk::restore_non_block_mode(handle, flag);
  return ret; // return -1 if no data in tcp stack buff under non-block mode
}
int ndk::send(ndk_handle handle, 
              const void *buff, 
              size_t len, 
              int flags, 
              const time_value *timeout) 
{
  int ret = 0;
  if (timeout == 0)
  {
    do{
      ret = ::send(handle, buff, len, flags);
    }while (ret == -1 && errno == EINTR);
    return ret;
  }

  // set nonblock mode
  int flag = 0;
  ndk::record_and_set_non_block_mode(handle, flag);

  // try it first
  ret = ::send(handle, buff, len, flags);
  if (ret == -1 
      &&(errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS))
  {
    // timeout
    if (*timeout > time_value::zero)
    {
      if (ndk::handle_read_ready(handle, timeout) > 0) 
        // <= 0 timeout not do send 
        // > 0 try it again
        ret = ::send(handle, buff, len, flags);
    }
  }
  // reset to block mode
  ndk::restore_non_block_mode(handle, flag);
  return ret;
}
int ndk::send_n(ndk_handle handle, 
                const void *buff, 
                size_t len, 
                int flags, 
                const time_value* timeout) 
{
  size_t send_bytes = 0;
  int error = 0;
  int result = 0;
  // set nonblock mode
  int flag = 0;
  if (timeout != 0)
    ndk::record_and_set_non_block_mode(handle, flag);
  for (int n = 0; send_bytes < len; send_bytes += n)
  {
    n = ::send(handle, 
               static_cast<const char *>(buff) + send_bytes, 
               len - send_bytes, 
               flags);
    if (n <= 0)  // peer closed or error
    {
      // check for possible blocking.
      if (n == -1 &&
          (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS))
      {
        // note msec not be modifyed. so not exact
        int ret = ndk::handle_write_ready(handle, timeout);  
        if (ret > 0) // time out or not ready handle
        {
          n = 0;  // !!
          continue;
        }
      }else if (n == -1 && errno == EINTR) 
      {
        n = 0;  // !!
        continue;  
      }else
        error = 1;

      result = n;
      break;
    }
  }
  // reset to block mode
  if (timeout != 0)
    ndk::restore_non_block_mode(handle, flag);
  if (error)
    return result;
  return send_bytes;
}
int ndk::sendv(ndk_handle handle, 
               const iovec iov[], 
               size_t count, 
               const time_value *timeout) 
{
  int ret = 0;
  if (timeout == 0)
  {
    do{
      ret = ::writev(handle, iov, count);
    }while (ret == -1 && errno == EINTR);
    return ret;
  }

  // set nonblock mode
  int flag = 0;
  ndk::record_and_set_non_block_mode(handle, flag);

  // try it first
  ret = ::writev(handle, iov, count);
  if (ret == -1 && 
      (errno == EAGAIN 
       || errno == EWOULDBLOCK 
       || errno == ENOBUFS))
  {
    // timeout
    if (*timeout > time_value::zero)
    {
      if (ndk::handle_write_ready(handle, timeout) > 0) 
        // <= 0 timeout not do writev 
        // try it again
        ret = ::writev(handle, iov, count);
    }
  }
  // reset to block mode
  ndk::restore_non_block_mode(handle, flag);
  return ret;
}
int ndk::sendv_n(ndk_handle handle, 
                 const iovec iov_[], 
                 size_t count, 
                 const time_value *timeout) 
{
  ssize_t n = 0;
  size_t send_bytes = 0;
  int result = 0;
  int error  = 0;
#if IOVEC_SECURITY_IO
  // for bak
  struct iovec *iov = const_cast<iovec*>(iov_);
  struct iovec *iov_bak =(struct iovec*)::malloc(sizeof(struct iovec) * count);
  for (size_t i = 0; i < count; ++i)
    iov_bak[i] = iov[i]; 
  bool bfull = true;
#else
  struct iovec *iov = const_cast<iovec*>(iov_);
#endif
  int flag = 0;
  if (timeout != 0)
    ndk::record_and_set_non_block_mode(handle, flag);
  for (size_t s = 0; s < count;)
  {
    n = ::writev(handle, iov + s, count - s);
    if (n == 0 ||  // peer closed
        n == -1)  // check for errors
    {
      // check for possible blocking.
      if (n == -1 &&
          (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS))
      {
        // note msec not be modifyed. so not exact
        int ret = ndk::handle_write_ready(handle, timeout);  
        if (ret > 0) 
        {
          n = 0;  // !!
          continue;
        }
      }else if (n == -1 && errno == EINTR) 
      {
        n = 0;  // !!
        continue;  
      }else 
        error = 1;

      result = n;
      break;
    }
    for (send_bytes += n; 
         s < count && n >= static_cast<int>(iov[s].iov_len); 
         ++s)
      n -= iov[s].iov_len;
#if IOVEC_SECURITY_IO
    if (n != 0)
    {
      bfull = false;
      char *base = static_cast<char *>(iov[s].iov_base);
      iov[s].iov_base = base + n;
      iov[s].iov_len  = iov[s].iov_len - n;
    }
#endif
  }
#if IOVEC_SECURITY_IO
  if (!bfull)
  {
    for (size_t i = 0; i < count; ++i)
      iov[i] = iov_bak[i]; 
  }
  ::free(iov_bak);
#endif
  // reset to block mode
  if (timeout)
    ndk::restore_non_block_mode(handle, flag);
  return send_bytes;
}
int ndk::sendto(ndk_handle handle, 
                const void *buff, 
                size_t len, 
                int flags, 
                const struct sockaddr *to,
                int tolen,
                const time_value *timeout) 
{
  int ret = 0;
  if (timeout == 0)
  {
    do{
      ret = ::sendto(handle, buff, len, flags, to, tolen);
    }while (ret == -1 && errno == EINTR);
    return ret;
  }

  // set nonblock mode
  int flag = 0;
  ndk::record_and_set_non_block_mode(handle, flag);

  // try it first
  ret = ::sendto(handle, buff, len, flags, to, tolen);
  if (ret == -1 
      &&(errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS))
  {
    if (*timeout > time_value::zero)
    {
      if (ndk::handle_read_ready(handle, timeout) > 0) 
        // <= 0 timeout not do send 
        // > 0 try it again
        ret = ::sendto(handle, buff, len, flags, to, tolen);
    }
  }
  // reset to block mode
  ndk::restore_non_block_mode(handle, flag);
  return ret;
}
int ndk::read(ndk_handle handle, 
              void *buff, 
              size_t len) 
{
  int ret = 0;
  do{
    ret = ::read(handle, buff, len);
  }while (ret == -1 && ret == EINTR);
  return ret;
} 
int ndk::read_n(ndk_handle handle, 
                void *buff, 
                size_t len) 
{
  size_t read_bytes = 0;
  for (int n = 0; read_bytes < len; read_bytes += n)
  {
    n = ::read(handle, 
               static_cast<char *>(buff) + read_bytes, 
               len - read_bytes);
    if (n == -1 || n == 0)  // check for errors
    {
      return n;
    }
  }
  return read_bytes;
}
int ndk::write(ndk_handle handle, 
               const void *buff, 
               size_t len) 
{
  int ret = 0;
  do{
    ret = ::write(handle, buff, len);
  }while (ret == -1 && ret == EINTR);

  return ret;
} 
int ndk::write_n(ndk_handle handle, 
                 const void *buff, 
                 size_t len) 
{
  if (handle == NDK_INVALID_HANDLE)
    return -1;
  size_t write_bytes = 0;
  for (int n = 0; write_bytes < len; write_bytes += n)
  {
    n = ::write(handle, 
                static_cast<const char *>(buff) + write_bytes, 
                len - write_bytes);
    if (n == 0 || n == -1) 
      return n;
  }
  return write_bytes;
}
int ndk::bind_port(ndk_handle handle, 
                   size_t ip_addr, 
                   int addr_family)
{
  inet_addr addr;
  addr.set(0, ip_addr);
  unused_args(addr_family);
  unsigned short upper_limit = MAX_DEFAULT_PORT;
  // We have to select the port explicitly.
  for (;;)
  {
    addr.set(upper_limit, ip_addr);
    if (::bind(handle,(sockaddr *)addr.get_addr(),
               addr.get_addr_size()) == 0)
      return 0;
    else if (errno != EADDRINUSE)
      return -1;
    else
      --upper_limit;
  }
  return -1;
}
int ndk::get_cpu_count(void)
{
#if defined NDK_WIN32
  SYSTEM_INFO sysinfo;
  ::GetSystemInfo(&sysinfo);
  return sysinfo.dwNumberOfProcessors;
#elif defined NDK_LINUX
  int ret = sysconf(_SC_NPROCESSORS_CONF) ;
  if (ret == -1) ret = 1;
  return ret;
#endif
}
// Quote from <Apache Portable Run-time libraries>
char *ndk::strtok(char *str, const char *sep, char **last)
{
  char *token;

  if (!str)           /* subsequent call */
    str = *last;      /* start where we left off */

  /* skip characters in sep(will terminate at '\0') */
  while (*str && strchr(sep, *str))
    ++str;

  if (!*str)          /* no more tokens */
    return NULL;

  token = str;

  /** 
   * skip valid token characters to terminate token and
   * prepare for the next call(will terminate at '\0) 
   */
  *last = token + 1;
  while (**last && !strchr(sep, **last))
    ++*last;

  if (**last) {
    **last = '\0';
    ++*last;
  }

  return token;
}
char *ndk::strstrip(const char *in_str, char *out_str, int len)
{
  const char *last  = 0;
  const char *begin = 0;

  if (in_str == 0 || out_str == 0 || len <= 0) 
    return 0;

  while (isspace((int)*in_str) && *in_str) 
    in_str++;
  begin = in_str;

  for (; *in_str; ++in_str)
  {
    if (isspace((int)*in_str) == 0)
      last = in_str;
  }
  int l = last - begin + 1;
  if (l < len)
    len = l;
  strncpy(out_str, begin, len);
  return out_str ;
}
char *ndk::strstrip_all(const char *in_str, char *out_str, int len)
{
  if (in_str == 0 || out_str == 0) 
    return 0;

  char *s = out_str;
  for (int i = 0; *in_str && i < len; ++in_str)
  {
    if (isspace((int)*in_str) == 0)
      *s++ = *in_str;
  }
  return s;
}

