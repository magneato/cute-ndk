#include "errno.h"

#include <cassert>

const char *strerr[] = 
{
  "",

  // == PULL ERROR
  "URL for pulling is invalid!",
  "Connect host failed!",
  "Response error!",
  "Not support multi pull session for one URL!",

  // == CLIENT ERROR
  "Response to client failed!",
  "Send stream data failed!",

  // == IO ERROR
  "Read file error!",
  "Open file failed!",
  "No space left on device!"
  ""
};

const char *my_strerr(const int err)
{
  assert(err > ERR_FIRST_INDEX && err < ERR_LAST_INDEX);
  return strerr[err - ERR_FIRST_INDEX];
}
