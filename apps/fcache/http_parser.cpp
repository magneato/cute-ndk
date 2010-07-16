#include "http_parser.h"

#include <string.h>
#include <ctype.h>

char *http_parser::get_value(const char *header,
                             const char *key,
                             size_t key_len,
                             char **v_end)
{
  char *key_p = ::strstr(const_cast<char *>(header), key);
  if (key_p == 0) return 0;
  key_p = ::strchr(key_p + key_len, ':');
  if (key_p == 0 || *(key_p + 1) == '\r') return 0;

  ++key_p;
  key_p += ::strspn(key_p, " \t");

  size_t len = ::strcspn(key_p, "\r\n");
  if (len == 0) return 0;

  *v_end = key_p + len;

  while (*v_end > key_p && isspace(**v_end))
    --(*v_end);
  ++(*v_end);
  return key_p;
}
char *http_parser::get_uri(const char *uri_begin,
                           char **uri_end)
{
  uri_begin += ::strspn(uri_begin, " \t");
  size_t len = ::strcspn(uri_begin, " \t");
  if (len == 0) return 0;

  *uri_end = const_cast<char *>(uri_begin) + len;
  return const_cast<char *>(uri_begin);
}
