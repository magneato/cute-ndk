// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-07-04 22:01
 */
//========================================================================

#ifndef HTTP_PARSER_H_
#define HTTP_PARSER_H_

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

  //
};

#endif // HTTP_PARSER_H_

