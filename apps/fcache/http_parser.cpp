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

  if (v_end != 0)
  {
    *v_end = key_p + len;

    while (*v_end > key_p && isspace(**v_end))
      --(*v_end);
    ++(*v_end);
  }
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
void http_parser::get_host_in_url(const char *url,
                                  std::string &host)
{
  char *p = ::strstr(url, "://");
  if (p == 0) return;
  p += 3;

  char *p1 = ::strchr(p, '/');
  if (p1 == 0)
  {
    host.assign(p);
  }else 
  {
    host.assign(p, p1 - p);
  }
}
void http_parser::get_uri_in_url(const char *url,
                                 std::string &uri)
{
  char *p = ::strstr(url, "://");
  if (p == 0) return;
  p += 3;

  char *p1 = ::strchr(p, '/');
  if (p1 != 0)
    uri.assign(p1);
  else
    uri = "/";
}
void http_parser::get_host_port(const char *url,
                                std::string &host,
                                int &port)
{
  char *p = ::strstr(url, "://");
  if (p == 0) return;
  p += 3;

  char *p1 = ::strchr(p, '/');
  char *p2 = ::strchr(p, ':');
  if (p1 == 0)
  {
    if (p2 == 0)
      host.assign(p);
    else
    {
      host.assign(p, p2 - p);
      if (*(p2 + 1) != 0)
        port = ::atoi(p2 + 1);
    }
  }else 
  {
    if (p2 == 0 || p2 > p1)
      host.assign(p, p1 - p);
    else
    {
      host.assign(p, p2 - p);
      if (p2 + 1 != p1)
        port = ::atoi(p2 + 1);
    }
  }
}
int http_parser::get_content_range(const char *str,
                                   int64_t &begin_pos,
                                   int64_t &end_pos,
                                   int64_t &entity_length)
{
  char *end_ptr = 0;
  char *begin_ptr = http_parser::get_value(str, 
                                           "Content-Range", 
                                           sizeof("Content-Range") - 1,
                                           &end_ptr);
  if (begin_ptr == 0)
    return -1;

  char *value_ptr = begin_ptr + ::strspn(begin_ptr, "bytes \t");
  // 0-1234/23434
  if (isdigit(value_ptr[0]))
  {
    begin_pos = ::strtoll(value_ptr, &end_ptr, 10);
    char *sep = ::strchr(value_ptr + 1, '-');
    if (sep == 0) 
      return -1;
    if (isdigit(*(sep + 1)))
    {
      end_pos = ::strtoll(sep + 1, &end_ptr, 10);
    }else
      end_pos = -1;
  }else if (value_ptr[0] == '-')
  {
    begin_pos = 0;
    if (isdigit(value_ptr[1]))
      end_pos = ::strtoll(value_ptr + 1, &end_ptr, 10);
    else
      end_pos = -1;
  }else
    return -1;

  char *sep = ::strchr(value_ptr, '/');
  if (sep == 0)
  {
    return -1;
  }
  if (isdigit(*(sep + 1)))
    entity_length = ::strtoll(sep + 1, &end_ptr, 10);
  else
    return -1;
  return 0;
}

