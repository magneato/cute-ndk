#include "errno.h"
#include "http_parser.h"
#include "file_manager.h"
#include "pull_sessionmgr.h"
#include "push_sessionmgr.h"
#include "pull_http_agent.h"
#include "handle_pull_file.h"
#include "queue_pull_sessionmgr.h"

#include <ndk/logger.h>
#include <ndk/inet_addr.h>
#include <ndk/connector.h>

extern ndk::connector<pull_agent> *g_connector;
static ndk::logger* pull_log = ndk::log_manager::instance()->get_logger("root.pull");

int handle_pull_file(const std::string &url,            // client request url.
                     const std::string &url_for_pull,   // url for pull
                     int sid,             // push sessionid
                     int64_t begin_pos,   // client request begin pos
                     int64_t end_pos)     // client request end pos
{
  int port = 80;
  std::string host;
  http_parser::get_host_port(url_for_pull.c_str(), host, port);
  if (host.empty() || port == 0)
  {
    pull_log->error("Invalid url [%s]", 
                    url_for_pull.c_str());
    return -PULL_ERR_URL_INVALID;
  }

  // create new pull session.
  int psid = pull_sessionmgr::instance()->alloc_sessionid();
  pull_session_ptr psession(new pull_session(psid));
  psession->request_url(url);
  psession->url_for_pulling(url_for_pull);
  psession->range_begin_pos(begin_pos);
  psession->range_end_pos(end_pos);
  psession->push_session_id(sid);
  pull_sessionmgr::instance()->insert(psession);

  if (queue_pull_sessionmgr::instance()->find(url) == 0)
  {
    pull_log->trace("multi pull file [%s]", url.c_str());
    handle_pull_file_failed("Not Support Multi Pull Session For One URL",
                            psession.get(),
                            1);
    return -PULL_MULTI_PULL_ERROR;
  }
  queue_pull_sessionmgr::instance()->insert(url);
  pull_log->trace("pull file [%s]...", url.c_str());
  // 
  if (1)//pbuffer == 0)
  {
    // connect to original server.
    ndk::inet_addr remote_addr(port, host.c_str());
    ndk::time_value timeout(3, 0);
    int result = g_connector->connect(new pull_http_agent(psession,
                                                          psession->range_begin_pos(),
                                                          psession->range_end_pos()),
                                      remote_addr,
                                      &timeout,
                                      ndk::inet_addr::addr_any,
                                      64*1024);
    if (result != 0)
    {
      pull_log->debug("connect [%s:%d] failed! [%s]", 
                 host.c_str(), 
                 port, 
                 strerror(errno));
      return -PULL_ERR_CONNECT_HOST_FAILED;
    }
  }
  return 0;
}
int handle_pull_file_ok(pull_session *pull_ss,
                        const std::string &url,
                        int64_t entity_length)
{
  // create new file info.
  file_info_ptr fileinfo = file_manager::instance()->find(url);
  if (!fileinfo)
  {
    fileinfo = file_info_ptr(new file_info());
    fileinfo->length(entity_length);
    fileinfo->url(url);
    std::string filename = url.substr(url.find_last_of("/") + 1);
    fileinfo->filename(filename);
    file_manager::instance()->insert(fileinfo);
  }

  // response to client.
  int push_sid = pull_ss->push_session_id();
  push_session_ptr push_ss = push_sessionmgr::instance()->find(push_sid);
  if (!push_ss)
  {
    pull_log->error("not found push session [%d]!",
                    push_sid);
    return -1;
  }
  //
  queue_pull_sessionmgr::instance()->remove(push_ss->request_url());

  int resp_code = 200;
  if (push_ss->range_begin_pos() != 0
      || push_ss->range_end_pos() != -1)
    resp_code = 206;

  int64_t begin_pos = push_ss->range_begin_pos();
  int64_t end_pos = push_ss->range_end_pos();
  if (end_pos == -1)
    end_pos = fileinfo->length() - 1;
  push_ss->content_length(end_pos - begin_pos + 1);
  int result = push_ss->client()->response_to_client(resp_code, 
                                                     push_ss->range_begin_pos(),
                                                     push_ss->range_end_pos() == -1
                                                     ? fileinfo->length() - 1
                                                     : push_ss->range_end_pos(),
                                                     entity_length,
                                                     fileinfo->mtime());
  if (result != 0)
  {
    pull_log->error("response error");
    push_ss->client()->session_desc(my_strerr(CLT_ERR_RESPONSE_FAILED));
    push_ss->client()->close();
    return -1;
  }
  if (push_ss->client()->http_method() == http_client::HTTP_HEAD)
  {
    push_ss->client()->session_desc("Get Resource Info!");
    push_ss->client()->close();
    return -1;
  }

  /**
   * If file size is zero, then set state to 'completed'.
   * We still storge this file info.
   */
  if (entity_length == 0)
  {
    fileinfo->status(file_info::ST_COMPLETED);
    push_ss->client()->close();
  }
  return 0;
}
int handle_pull_file_failed(const char *desc,
                            pull_session *pull_ss,
                            int multi_pull)
{
  // response to client.
  int push_sid = pull_ss->push_session_id();
  push_session_ptr push_ss = push_sessionmgr::instance()->find(push_sid);
  if (push_ss)
  {
    if (!multi_pull)
      queue_pull_sessionmgr::instance()->remove(push_ss->request_url());
    push_ss->client()->session_desc(desc);
    push_ss->client()->response_to_client(404);
    push_ss->client()->close();
  }else
  {
    pull_log->error("not found push session [%d]!",
                    push_sid);
  }
  return 0;
}
int handle_pull_file_over(file_info *finfo, pull_session *)
{
  // check file is completed or not!
  return 0;
}
#if 0
int check_shared_pipe_buffer()
{
  int chunk_id = begin_pos / PIPE_CHUNK_SIZE;
  // check this chunk is pulling or not.
  int psid = pull_file_waiting_queue::instance()->find(url, chunk_id);
  if (psid != -1) 
  {
    // had create a pull session to pull this chunk.
    
    pull_session_ptr pss = pull_sessionmgr::instance()->find(psid);
    if (!pss)
    {
      log->warning("pull [%s] find pull queued-job but not find "
                   "pull session [%d]!",
                   url.c_str(),
                   psid);
    }else
    {
      /**
       * notify the observers that request this chunk.
       * when <pss> pull ok (get the first data block) then notify all 
       * the observers.
       */
      pss->add_pull_observer(sid);
      return 0;
    }
  }else
  {
    /** 
     * find pipe buffer in pipe list.
     */
    pipe_buffer_ptr pbuffer = pipe_buffermgr::instance()->find(url, chunk_id);
    if (pbuffer)
    {
      // 
      result = response_to_client();
      if (result != 0)
      {
        notify_close();
        return 0;
      }

      transfer_agent *trans_agent = new shared_pipe_buffer_transfer();

      result = handle_start_transfer_data(trans_agent,
                                          this->session_id_,
                                          begin_pos, 
                                          end_pos,
                                          this->content_length_,
                                          this);

      return 0;
    }
  }
  return -1;
}
#endif
