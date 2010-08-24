// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-24 22:01
 */
//========================================================================

#ifndef PUSH_SESSION_H_
#define PUSH_SESSION_H_

#include <boost/shared_ptr.hpp>

#include "session.h"
#include "http_client.h"

/**
 * @class push_session
 * 
 * @brief
 */
class push_session : public session
{
public:
  push_session(int sid)
    : session(sid),
    curr_output_bdwidth_(0),
    output_bdwidth_limit_(0),
    client_(0)
  { }

  ~push_session()
  { }

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

  // == Method
  void release();
private:
  int curr_output_bdwidth_;
  int output_bdwidth_limit_;

  http_client *client_;

  std::string client_address_;
};
typedef boost::shared_ptr<push_session> push_session_ptr;

#endif // PUSH_SESSION_H_

