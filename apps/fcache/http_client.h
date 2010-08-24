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
  enum
  {
    HTTP_UNKNOW, 
    HTTP_GET,  
    HTTP_HEAD  
  };
public:
  http_client()
    : session_id_(-1),
    dispatch_data_task_idx_(0),
    recv_msg_ok_(0),
    transfer_bytes_(0),
    bytes_to_send_(4096),
    content_length_(0),
    timer_id_(-1),
    http_method_(HTTP_UNKNOW),
    response_code_(0),
    session_desc_("~"),
    uri_("~"),
    recv_buff_(0)
  {  }

  virtual ~http_client();

  inline virtual void set_remote_addr(const ndk::inet_addr &remote_addr)
  { this->remote_addr_ = remote_addr; }

  virtual int open(void *arg);

  virtual int handle_input(ndk::ndk_handle );

  virtual int handle_timeout(const void *, const ndk::time_value &now);

  virtual int handle_close(ndk::ndk_handle , ndk::reactor_mask m);

  int response_to_client(int , int64_t = 0, int64_t = 0, int64_t = 0, time_t = 0); 

  void transfer_bytes(int64_t bytes)
  { this->transfer_bytes_ = bytes; }

  void session_desc(const std::string &desc)
  { this->session_desc_ = desc; }

  int http_method()
  { return this->http_method_; }

  void show_status();

protected:
  int handle_data();

  int get_range(const char *, int64_t &, int64_t &);

  int handle_request(const char *http_header);

  char *gmttime_to_str(time_t now, char *str, size_t len);

  int check_modified(const char *http_header,
                     const time_t last_modified_time,
                     const std::string &etag);

  int parse_params(const char *param_p,
                   int &bandwidth,
                   std::string &url);
protected:
  int session_id_;
  int dispatch_data_task_idx_;
  int recv_msg_ok_;
  int64_t transfer_bytes_;
  int bytes_to_send_;
  int64_t content_length_;
  int timer_id_;

  int http_method_;
  int response_code_;
  std::string session_desc_;

  std::string uri_;

  ndk::time_value begin_time_;
  ndk::message_block *recv_buff_;
  ndk::inet_addr remote_addr_;
};

#endif // HTTP_CLIENT_H_

