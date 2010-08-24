// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-07-02 16:01
 */
//========================================================================

#ifndef PULL_HTTP_AGENT_H_
#define PULL_HTTP_AGENT_H_

#include "file_info.h"
#include "pull_agent.h"
#include "pull_session.h"

#include <ndk/message_block.h>

class pipe_buffer;
class transfer_agent;

/**
 * @class pull_http_agent
 * 
 * @brief
 */
class pull_http_agent : public pull_agent
{
public:
  pull_http_agent(const pull_session_ptr &pull_ss,
                  int64_t begin_pos,
                  int64_t end_pos);

  virtual ~pull_http_agent();

  virtual int open(void *);
protected:
  int send_reqest(const std::string &uri,
                  const std::string &host,
                  int64_t &begin_pos,
                  int64_t &end_pos);

  virtual int handle_input(ndk::ndk_handle );

  virtual int handle_timeout(const void *, const ndk::time_value &now);

  virtual int handle_close(ndk::ndk_handle h, ndk::reactor_mask mask);

  pipe_buffer *create_pipe_buffer(int64_t entity_length,
                                  int chunk_id);

  int handle_response(void);

  int handle_content(void);
protected:
  int timer_id_;

  ndk::ndk_handle file_handle_;

  int suspend_; 

  int chunk_id_;

  int error_code_;

  int connect_ok_;

  int pull_info_ok_;
  int pull_content_ok_;
  int to_recv_header_;

  int recved_first_package_;

  int64_t begin_pos_;
  int64_t end_pos_;
  int64_t content_length_;
  int64_t remaind_length_;

  int64_t recv_bytes_;

  ndk::message_block *recv_buff_;

  pipe_buffer *pipe_buffer_;

  transfer_agent *transfer_agent_;

  std::string error_desc_;
  pull_session_ptr pull_session_;

  file_info_ptr file_info_;
};

#endif // PULL_HTTP_AGENT_H_
