// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 17:24
 */
//========================================================================

#ifndef NDK_CONFIG_BASE_H_
#define NDK_CONFIG_BASE_H_

# define MAX_HOSTNAME_LEN                    255

# define MAX_DEFAULT_PORT                    65535

// For <timer_queue>
# define NDK_DEFAULT_MAX_TIMERS              1024

// If there is a large amount of active fds, this operation can avoid 
// starvation.
# define MAX_REACTOR_PROCESS_FDS_ONE_TIME    32

// For listen
# define NDK_DEFAULT_BACKLOG                 128

#endif // NDK_CONFIG_BASE_H_

