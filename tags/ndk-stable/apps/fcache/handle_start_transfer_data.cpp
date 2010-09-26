#include "file_info.h"
#include "serial_file_transfer.h"
#include "mem_cache_transfer.h"
#include "dispatch_data_task.h"

extern size_t g_min_mem_cache_size;
extern size_t g_max_mem_cache_size;
extern int64_t g_cache_file_requests;
extern int64_t g_disk_file_reqeusts;
extern int64_t g_hit_disk;
extern int64_t g_payload;
extern dispatch_data_task *g_dispatch_data_task;

#if 0
int handle_prepare_to_transfer_data(int response_code,
                                    int64_t begin_pos,
                                    int64_t end_pos,
                                    int64_t content_length,
                                    file_info_ptr &fileinfo,
                                    http_client *hc,
                                    transfer_agent *&trans_agent)
{
  // select transfer agent.
  int result = 0;
  if (trans_agent == 0
      && (fileinfo->length() <= (int64_t)g_max_mem_cache_size 
          && fileinfo->length() >= (int64_t)g_min_mem_cache_size
         )
     )
  {
    ++g_cache_file_requests;
    trans_agent = new mem_cache_transfer(begin_pos, 
                                         content_length);
    result = trans_agent->open(fileinfo);
    if (result != 0)
    {
      // cache manager is full.
      delete trans_agent;
      trans_agent = 0;
    }else
    {
      hc->session_desc(hit_cache);
    }
  }

  // direct read file from disk.
  if (trans_agent == 0)
  {
    ++g_disk_file_reqeusts;
    trans_agent = new serial_file_transfer(begin_pos, 
                                           content_length);
    if (trans_agent->open(fileinfo) != 0)
    {
      delete trans_agent;
      trans_agent = 0;
      return -PULL_ERR_OPEN_FILE_FAILED;
    }else
    {
      hc->session_desc(hit_disk);
      ++g_hit_disk;
    }
  }
  return 0;
}
#endif
int handle_start_transfer_data(transfer_agent *trans_agent,
                               int sid,
                               int bandwidth,
                               int64_t /*begin_pos*/,
                               int64_t /*end_pos*/,
                               int64_t content_length,
                               http_client *hc)
{
  trans_agent->start_transfer();
  dispatch_data_task::dispatch_job *job = 
    new dispatch_data_task::dispatch_job(dispatch_data_task::dispatch_job::add_job);
  job->session_id_ = sid;
  job->transfer_agent_ = trans_agent;
  job->client_ = hc;
  job->content_length_ = content_length;
  job->bandwidth_ = bandwidth;
  g_dispatch_data_task[0].push_job(job);
  ++g_payload;
  return 0;
}
