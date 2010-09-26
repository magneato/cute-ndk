// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-08-11 11:07
 */
//========================================================================

#ifndef ERRNO_H_
#define ERRNO_H_

enum
{
  ERR_FIRST_INDEX                       = 1000,

  // == PULL ERROR
  PULL_ERR_URL_INVALID,

  PULL_ERR_CONNECT_HOST_FAILED,

  PULL_ERR_RESPONSE_ERROR,

  PULL_MULTI_PULL_ERROR,

  // == CLIENT ERROR                    
  CLT_ERR_RESPONSE_FAILED,

  CLT_ERR_SEND_DATA_FAILED,

  // == IO ERROR
  IO_ERR_READ_FILE_ERROR,

  IO_ERR_NO_SPACE,

  PULL_ERR_OPEN_FILE_FAILED,


  ERR_LAST_INDEX
};

extern const char *my_strerr(const int err);
#endif // ERRNO_H_

