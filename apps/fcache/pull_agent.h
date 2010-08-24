// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-07-02 16:01
 */
//========================================================================

#ifndef PULL_AGENT_H_
#define PULL_AGENT_H_

#include <ndk/svc_handler.h>

/**
 * @class pull_agent
 * 
 * @brief
 */
class pull_agent : public ndk::svc_handler
{
public:
  pull_agent() { }

  virtual ~pull_agent() {}

protected:
  typedef ndk::svc_handler super;
};

#endif // PULL_AGENT_H_
