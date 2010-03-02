#include "raw_io_data_prod.h"

ndk::message_block *raw_io_data_prod::get_data_block()
{
  if (this->status_ == opt_inprogress)
    return 0;
  else if (this->status_ == opt_eof || this->status_ == opt_error)
    return 0;
  else if (this->status_ == opt_holding)
  {
    if (this->data_block_->length() == 0
    return this->data_block_;
  }
  return 0;
}
