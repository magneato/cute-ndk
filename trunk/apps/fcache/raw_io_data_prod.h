// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-07-02 11:15
 */
//========================================================================

#ifndef RAW_IO_DATA_PROD_H_
#define RAW_IO_DATA_PROD_H_

#include "data_productor.h"

/**
 * @class raw_io_data_prod
 * 
 * @brief
 */
class raw_io_data_prod : public data_productor
{
public:
  raw_io_data_prod()
  {
  }

  virtual ~raw_io_data_prod()
  {
  }

  // 
  virtual int status();

  //
  virtual ndk::message_block *get_data_block();
protected:

};

#endif // RAW_IO_DATA_PROD_H_

