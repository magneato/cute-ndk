// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-24 23:47
 */
//========================================================================

#ifndef HTTP_CONNECTION_H_
#define HTTP_CONNECTION_H_

#include <ndk/svc_handler.h>

/**
 * @class http_connection
 * 
 * @brief
 */
class http_connection : public ndk::svc_handler
{
public:
  http_connection();

  virtual ~http_connection();
  //
  virtual int handle_input(ndk::ndk_handle );

  //
  virtual int handle_close(ndk::ndk_handle , ndk::reactor_mask);
protected:
  int handle_data(message_block *buff);

  //
};

#endif // HTTP_CONNECTION_H_

