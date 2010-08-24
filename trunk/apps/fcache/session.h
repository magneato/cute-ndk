// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-24 22:01
 */
//========================================================================

#ifndef SESSION_H_
#define SESSION_H_

#include <string>

/**
 * @class session
 * 
 * @brief
 */
class session
{
public:
  session(int sid)
    : session_id_(sid),
      range_begin_pos_(0),
      range_end_pos_(-1),
      content_length_(0)
  { }

  virtual ~session()
  { }

  inline void session_id(int sid)
  { this->session_id_ = sid; }

  inline int session_id()
  { return this->session_id_; }

  inline std::string &request_url()
  { return this->request_url_; }

  inline void request_url(const std::string &val)
  { this->request_url_ = val; }

  inline int64_t range_begin_pos(void)
  { return this->range_begin_pos_; }

  inline void range_begin_pos(int64_t val)
  { this->range_begin_pos_ = val; }

  inline int64_t range_end_pos(void)
  { return this->range_end_pos_; }

  inline void range_end_pos(int64_t val)
  { this->range_end_pos_ = val; }

  inline int64_t content_length(void)
  { return this->content_length_; }

  inline void content_length(int64_t val)
  { this->content_length_ = val; }
  //
  virtual void release() = 0;
protected:
  int session_id_;

  // 
  int64_t range_begin_pos_;
  int64_t range_end_pos_;

  int64_t content_length_;

  std::string request_url_;
};

#endif // SESSION_H_

