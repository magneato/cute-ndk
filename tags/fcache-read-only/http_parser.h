// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-07-04 22:01
 */
//========================================================================

#ifndef HTTP_PARSER_H_
#define HTTP_PARSER_H_

#include <string>
#include <stdlib.h>

/**
 * @class http_parser
 * 
 * @brief
 */
class http_parser
{
public:
  // 
  static char *get_value(const char *header, 
                         const char *key,
                         size_t key_len,
                         char **v_end);

  //
  static char *get_uri(const char *uri_begin, char **uri_end);

  // http://www.g.cn:8080/
  static void get_host_in_url(const char *url, std::string &host);

  // http://www.g.cn:8080/index.html
  static void get_uri_in_url(const char *url, std::string &uri);

  // http://www.g.cn:8080/
  static void get_host_port(const char *url,
                            std::string &host,
                            int &port);

  // Content-Range: bytes 0-499/1000
  static int get_content_range(const char *str,
                               int64_t &begin_pos,
                               int64_t &end_pos,
                               int64_t &entity_length);
};

#endif // HTTP_PARSER_H_

