// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-08-10 23:06
 */
//========================================================================

#ifndef HANDLE_START_TRANSFER_DATA_H_
#define HANDLE_START_TRANSFER_DATA_H_

#include "file_info.h"

class http_client;
class transfer_agent;

#if 0
extern int handle_prepare_to_transfer_data(int response_code,
                                           int64_t begin_pos,
                                           int64_t end_pos,
                                           int64_t content_length,
                                           file_info_ptr &fileinfo,
                                           http_client *hc,
                                           transfer_agent *&trans_agent);
#endif

extern int handle_start_transfer_data(transfer_agent *trans_agent,
                                      int sid,
                                      int bandwidth,
                                      int64_t begin_pos,
                                      int64_t end_pos,
                                      int64_t content_length,
                                      http_client *hc);

#endif // HANDLE_START_TRANSFER_DATA_H_
