// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-07-02 11:14
 */
//========================================================================

#ifndef DATA_PRODUCTOR_H_
#define DATA_PRODUCTOR_H_

class prod_result
{
public:
  enum
  {
    opt_inprogress     = 1L << 0,
    opt_eof            = 1L << 1,
    opt_error          = 1L << 2,
    opt_holding        = 1L << 3
  };
  int error;
  ndk::message_block *data_block;
};
/**
 * @class data_productor
 * 
 * @brief
 */
class data_productor
{
public:
  data_productor()
    : status_(opt_error)
  { }

  virtual ~data_productor()
  { }

  virtual int open(void *) = 0;

  inline virtual int status() const
  { return this->status_; }

  // abs methods
  virtual ndk::message_block *get_data_block() = 0;
protected:
  volatile int status_;
};

#endif // DATA_PRODUCTOR_H_

