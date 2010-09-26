// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-07-27 23:34
 */
//========================================================================

#ifndef SERVICE_STATUS_H_
#define SERVICE_STATUS_H_

/**
 * @class service_status
 * 
 * @brief
 */
class service_status
{
public:
  service_status() { }

  void add_http_requests(void)
  { ++this->http_requests_; }

  void int http_requests(void)
  { return this->http_requests_; }

  //
protected:
  int http_requests_;
};

#endif // SERVICE_STATUS_H_
