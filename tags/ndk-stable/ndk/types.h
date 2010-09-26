// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 17:24
 */
//========================================================================

#ifndef NDK_TYPES_H_
#define NDK_TYPES_H_

#include "ndk/config.h"

#include <stdlib.h>
#include <pthread.h>        // for pthread_t

namespace ndk
{
// =======================================================================
// Type define 
// =======================================================================
  typedef int ndk_handle;
  typedef ndk_handle ndk_socket;
# define NDK_INVALID_HANDLE (-1)
# define SOCKET_ERROR       (-1)
  typedef pthread_t thread_t;
  typedef pthread_t hthread_t;
# define NULL_thread  0
  typedef void * thread_func_return_t; 
  typedef thread_func_return_t (*thread_func_t)(void*);

} // namespace ndk
#endif // NDK_TYPES_H_
