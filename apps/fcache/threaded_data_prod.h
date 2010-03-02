// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-07-11 19:27
 */
//========================================================================

#ifndef THREADED_DATA_PROD_H_
#define THREADED_DATA_PROD_H_

#include "data_productor.h"

/**
 * @class threaded_data_prod
 * 
 * @brief
 */
class threaded_data_prod : public data_productor
{
public:
  threaded_data_prod()
  {
  }

  virtual ~threaded_data_prod()
  {
  }

  // virtual methed.
  virtual prod_result get_data_block();
};

#endif // THREADED_DATA_PROD_H_

