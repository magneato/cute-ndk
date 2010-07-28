// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-24 22:01
 */
//========================================================================

#ifndef HTTP_SESSION_H_
#define HTTP_SESSION_H_

#include <string>

#include "http_client.h"

/**
 * @class http_session
 * 
 * @brief
 */
class http_session
{
public:
  http_session(int sid)
    : session_id_(sid),
    curr_output_bdwidth_(0),
    output_bdwidth_limit_(0),
    client_(0)
  { }

  ~http_session()
  { }

  inline void session_id(int sid)
  { this->session_id_ = sid; }

  inline int session_id()
  { return this->session_id_; }

  inline std::string &request_url()
  { return this->request_url_; }

  inline void request_url(const std::string &val)
  { this->request_url_ = val; }
   
  inline void curr_output_bdwidth(int bw)
  { this->curr_output_bdwidth_ = bw; }

  inline int curr_output_bdwidth()
  { return this->curr_output_bdwidth_; }

  inline void output_bdwidth_limit(int bw)
  { this->output_bdwidth_limit_ = bw; }

  inline int output_bdwidth_limit()
  { return this->output_bdwidth_limit_; }

  inline void client(http_client *c)
  { this->client_ = c; }

  inline http_client *client(void)
  { return this->client_; }

  inline std::string &client_address()
  { return this->client_address_; }

  inline void client_address(const std::string &val)
  { this->client_address_ = val; }
private:
  int session_id_;
  int curr_output_bdwidth_;
  int output_bdwidth_limit_;

  http_client *client_;

  std::string request_url_;
  std::string client_address_;
};

#endif // HTTP_SESSION_H_

