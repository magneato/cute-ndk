// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-05-24 22:01
 */
//========================================================================

#ifndef HTTP_CLIENT_H_
#define HTTP_CLIENT_H_

#include <ndk/svc_handler.h>
#include <ndk/cache_object.h>
#include <ndk/message_block.h>

#include <string>

/**
 * @class http_client
 * 
 * @brief
 */
class http_client : public ndk::svc_handler
{
public:
  http_client()
    : session_id_(-1),
    dispatch_data_task_idx_(0),
    recv_msg_ok_(0),
    send_bytes_(0),
    bytes_to_send_(4096),
    content_length_(0),
    timer_id_(-1),
    file_handle_(NDK_INVALID_HANDLE),
    cache_obj_(0),
    recv_buff_(0)
  {  }

  virtual ~http_client();

  inline virtual void set_remote_addr(const ndk::inet_addr &remote_addr)
  { this->remote_addr_ = remote_addr; }

  virtual int open(void *arg);

  virtual int handle_input(ndk::ndk_handle );

  virtual int handle_timeout(const void *, const ndk::time_value &now);

  virtual int handle_close(ndk::ndk_handle , ndk::reactor_mask m);

  int handle_data();

  int response_client(int , int64_t, int64_t, const std::string &);

  int show_status();
protected:
  int session_id_;
  int dispatch_data_task_idx_;
  int recv_msg_ok_;
  int send_bytes_;
  int bytes_to_send_;
  int content_length_;
  int timer_id_;
  ndk::ndk_handle file_handle_;
  ndk::cache_object* cache_obj_;
  ndk::time_value begin_time_;
  ndk::message_block *recv_buff_;
  ndk::inet_addr remote_addr_;
};

#endif // HTTP_CLIENT_H_

