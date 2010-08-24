// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-08-10 23:06
 */
//========================================================================

#ifndef HANDLE_PULL_FILE_H_
#define HANDLE_PULL_FILE_H_

#include "pull_session.h"

extern int handle_pull_file(const std::string &url,            // client request url.
                            const std::string &url_for_pull,   // url for pull
                            int sid,              // client sessionid
                            int64_t begin_pos,    // client request begin pos
                            int64_t end_pos);     // client request end pos

extern int handle_pull_file_ok(pull_session *pull_ss,
                               const std::string &url,
                               int64_t entity_length);

extern int handle_pull_file_failed(const char *desc,
                                   pull_session *pull_ss,
                                   int multi_pull);

extern int handle_pull_file_over(file_info *, pull_session *);
#endif // HANDLE_PULL_FILE_H_

