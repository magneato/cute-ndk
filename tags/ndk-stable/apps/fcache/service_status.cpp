int service_status::show_status(string &msg)
{
  static char requests[]     = "requests";
  static int  requests_w     = sizeof(requests) + 1;
  static char c_file_reqs[]  = "c_file_reqs";
  static int  c_file_reqs_w  = sizeof(c_file_reqs) + 1;
  static char d_file_reqs[]  = "d_file_reqs";
  static int  d_file_reqs_w  = sizeof(d_file_reqs) + 1;
  static char finished[]     = "finished";
  static int  finished_w     = sizeof(finished) + 1;
  static char hit_cache[]    = "hit_cache";
  static int  hit_cache_w    = sizeof(hit_cache) + 1;
  static char hit_disk[]     = "hit_disk";
  static int  hit_disk_w     = sizeof(hit_disk) + 1;
  static char hit_c_rate[]   = "hit_cache_rate";
  static int  hit_c_rate_w   = sizeof(hit_c_rate) + 1;
  static char payload[]      = "payload";
  static int  payload_w      = sizeof(payload) + 1;
  static char cache_mem_used[]  = "c_mem_used(mb)";
  static int  cache_mem_used_w  = sizeof(cache_mem_used) + 1;
  static char aio_mem_used[]  = "aio_mem_used(mb)";
  static int  aio_mem_used_w  = sizeof(aio_mem_used) + 1;

  std::ostringstream  ostr;
  ostr << std::setfill(' ')
    << std::setiosflags(std::ios::left);
  ostr << std::setw(requests_w) << requests
    << std::setw(finished_w) << finished
    << std::setw(c_file_reqs_w) << c_file_reqs 
    << std::setw(d_file_reqs_w) << d_file_reqs 
    << std::setw(hit_cache_w) << hit_cache
    << std::setw(hit_c_rate_w) << hit_c_rate
    << std::setw(hit_disk_w) << hit_disk
    << std::setw(payload_w) << payload
    << std::setw(cache_mem_used_w) << cache_mem_used
    << std::setw(aio_mem_used_w) << aio_mem_used
    << std::endl;
  int h_c_rate  = 0;
  if (g_finished > 0 && g_hit_cache > 0)
    h_c_rate = int((g_hit_cache * 100) / g_cache_file_requests);
  ostr << std::setw(requests_w) << g_requests
    << std::setw(finished_w) << g_finished
    << std::setw(c_file_reqs_w) << g_cache_file_requests
    << std::setw(d_file_reqs_w) << g_disk_file_reqeusts
    << std::setw(hit_cache_w) << g_hit_cache
    << std::setw(hit_c_rate_w) << h_c_rate
    << std::setw(hit_disk_w) << g_hit_disk
    << std::setw(payload_w) << g_payload
    << std::setw(cache_mem_used_w) << (g_cache_mem_used > 0 ? 
    g_cache_mem_used/1024/1024 : g_cache_mem_used)
    << std::setw(aio_mem_used_w) << file_io_cache_mgr->alloc_blocks()
    << std::endl << std::endl;
  msg = ostr.str();
  return 0;
}
