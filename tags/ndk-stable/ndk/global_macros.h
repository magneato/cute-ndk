// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 17:24
 */
//========================================================================

#ifndef NDK_GLOBAL_MACROS_H_
#define NDK_GLOBAL_MACROS_H_

#include "ndk/config.h"

// =======================================================================
// Bit operation macros 
// =======================================================================
#define NDK_BIT_ENABLED(WORD, BIT)  (((WORD) & (BIT)) != 0)
#define NDK_BIT_DISABLED(WORD, BIT) (((WORD) & (BIT)) == 0)

#define NDK_SET_BITS(WORD, BITS) (WORD |= (BITS))
#define NDK_CLR_BITS(WORD, BITS) (WORD &= ~(BITS))

// Efficiently returns the least power of two >= X...
#define NDK_POW(X) (((X) == 0)?1:(X-=1,X|=X>>1,X|=X>>2,X|=X>>4,X|=X>>8,X|=X>>16,(++X)))

// =======================================================================
// Useful macros
// =======================================================================
#ifndef NDK_DEBUG
# define NDK_ASSERT(log, _condition)
#else
# define NDK_ASSERT(log, _condition)		\
  do {					\
    if (!(_condition))			\
    {					\
      log->fatal("NDK_ASSERT : in file "	\
                 "`%s` on line `%d` : `%s` "	\
                 "assertion failed for '%s'"	\
                 "\n",				\
                 __FILE__,			\
                 __LINE__,			\
                 __PRETTY_FUNCTION__,		\
                #_condition);			\
      abort();				\
    }					\
  }while (false)
#endif

#if defined NDK_WIN32
# define NDK_SOCKCALL_RETURN(OP,TYPE,ERRNO)   \
  do {					\
    TYPE _result_ = (TYPE) OP;		\
    if (_result_ == ERRNO)			\
    {					\
      int ___ = ::WSAGetLastError ();	\
      errno = ___;			\
      return (TYPE) ERRNO;		\
    }else					\
    return _result_;			\
  }while (0)					
#elif defined NDK_LINUX
# define NDK_SOCKCALL_RETURN(OP,TYPE,ERRNO)	\
  do {					\
    return (TYPE) (OP);			\
  }while (0)
#endif  // endof if defined NDK_WIN32

// = For stack trace
#ifdef NDK_STRACE
# define STRACE(X) ndk::strace ____ (__FILE__,  __PRETTY_FUNCTION__, __LINE__)
#else
# define STRACE(X)
#endif

#ifdef NDK_RTLOG
# define NDK_LOG(X, ...) fprintf(stderr, X "\n", ##__VA_ARGS__)
#else
# define NDK_LOG(X, ...)
#endif

// = Port error numbers.
#if defined NDK_WIN32
# define EWOULDBLOCK	    WSAEWOULDBLOCK
# define EAGAIN	          EWOULDBLOCK
# define EINPROGRESS	    WSAEINPROGRESS
# define EINTR	          WSAEINTR
# define ETIMEDOUT	      WSAETIMEDOUT
# define ENOBUFS	        WSAENOBUFS
# define EINVAL	          WSAEINVAL
# define ECONNRESET	      WSAECONNRESET
# define EBADF	          WSAENOTSOCK
# define EADDRINUSE	      WSAEADDRINUSE
#endif

#endif // NDK_GLOBAL_MACROS_H_

