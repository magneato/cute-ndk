// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-06-24 22:01
 */
//========================================================================

#ifndef SESSION_H_
#define SESSION_H_

/**
 * @class session
 * 
 * @brief
 */
#define INVALID_SESSION_ID  -1

typedef int session_id_t;

class session
{
public:
  enum
  {
    closed         = 1L << 0,
    inprocess      = 1L << 1,
    transporting   = 1L << 2,
  };
  session()
  : status_(closed),
  session_id_(INVALID_SESSION_ID)
  {
  }

  virtual ~session()
  { }
  
  //
  void statist_total_output_flux(int val) = 0;
protected:
  int status_;

  session_id_t  session_id_;
  
  // 192.168.1.100
  std::string peer_addr_;

  // /media/xbox.wmv
  std::string uri_;
};

#endif // SESSION_H_

