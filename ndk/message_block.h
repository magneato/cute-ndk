// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 02:24
 */
//========================================================================

#ifndef NDK_MESSAGE_BLOCK_H_
#define NDK_MESSAGE_BLOCK_H_

#include <cstring>
#include <cstdio>
#include <stdint.h>

namespace ndk
{
  /**
   * @class message_block
   *
   * @brief 
   */
  class message_block
  {
  public:
    message_block(const size_t size)
      : release_data_(false),
        mb_data_(0),
        mb_size_(0),
        mb_type_(0),
        rd_ptr_(0),
        wr_ptr_(0),
        prev_(0),
        next_(0)
    {
      mb_data_ = ::new char[size];
      if (mb_data_) 
      {
        release_data_ = true;
        mb_size_ = size;
      }
    }

    message_block(const char *data_ptr, size_t size)
      : release_data_(false),
        mb_data_ (const_cast<char *>(data_ptr)),
        mb_size_(size),
        mb_type_(0),
        rd_ptr_(0),
        wr_ptr_(0),
        prev_(0),
        next_(0)
    { }

    virtual ~message_block()
    { }

    inline void *operator new (size_t bytes)
    { return ::new char[bytes]; }

    inline void operator delete (void *ptr)
    { ::delete [] ((char *)ptr); }

    // get message data pointer
    inline char *data(void)
    { return this->mb_data_; }

    // get message data len
    inline size_t size(void)
    { return this->mb_size_; }

    // message length is(wr_ptr - rd_ptr).
    inline size_t length(void) const
    { return this->wr_ptr_ - this->rd_ptr_; }

    // get the number of bytes available after the <wr_ptr_> in the
    // <message_block>
    inline size_t space(void) const
    { return this->mb_size_ > 0 ? this->mb_size_ - this->wr_ptr_ : 0; }

    inline int copy(const char *buf, size_t len)
    {
      if (len > this->space()) return -1;
      else
      {
        std::memcpy(this->wr_ptr(), buf, len);
        this->wr_ptr(len);
      }
      return 0;
    }

    // get messageblock type
    inline int data_type(void) const
    { return this->mb_type_; }

    // set messageblock type
    inline void data_type(const int type)
    { this->mb_type_ = type; }

    // pointer to the message_block directly ahead in the message_queue.
    // get link to next mb,
    inline message_block *next(void) const
    { return this->next_; }

    // set link to next mb,
    inline void next(message_block *mb)
    { this->next_ = mb; }

    // pointer to the message_block directly behind in the message_queue.
    // get link to prev mb,
    inline message_block *prev(void) const
    { return this->prev_; }

    // set link to prev mb,
    inline void prev(message_block *mb)
    { this->prev_ = mb; }

    // get the read pointer.
    inline char *rd_ptr(void) const
    { return this->mb_data_ + this->rd_ptr_; }

    // set the read pointer ahead n bytes.
    inline void rd_ptr(size_t n)
    { this->rd_ptr_ += n; }

    // get the write pointer.
    inline char *wr_ptr(void) const
    { return this->mb_data_ + this->wr_ptr_; }

    // set the write pointer ahead n bytes. This is used to compute
    // the <length> of a message.
    inline void wr_ptr(size_t n)
    { this->wr_ptr_ += n; }

    // release message data which is alloced by message_block, If the 
    // message_block is a link, the release all of them. 
    inline void release(void)
    {
      for (message_block *mt = 0, *mb = this; mb != 0; )
      {
        mt = mb; mb = mb->next(); mt->clean();
        delete mt;
      }
    }

    // sets the read and write pointers to align with the base.
    inline void reset(void)
    { this->rd_ptr_ = this->wr_ptr_ = 0; }

    // release the memory which is alloced by message_block,
    inline void clean(void)
    {
      if (this->mb_data_ && this->release_data_)
      {
        ::delete []this->mb_data_;
        this->mb_data_ = 0;
        this->mb_size_ = 0;
      }
    }

    // set '\0' to the special pos.
    // if <pos> is -1, the pos equal message_block::length().
    inline void set_eof(const int pos = -1)
    {
      if (pos == -1)
        this->mb_data_[this->length()] = '\0';
      else
        this->mb_data_[pos] = '\0';
    }

    // numeric operators
#define MESSAGE_BLOCK_READ_OPERATOR(type)               \
    inline message_block & operator >> (type &v)        \
    {                                                   \
      if (this->length() >= sizeof(type))               \
      {                                                 \
        std::memcpy(&v, this->rd_ptr(), sizeof(type));  \
        this->rd_ptr(sizeof(type));                     \
      }                                                 \
      return *this;                                     \
    }

#define MESSAGE_BLOCK_WRITE_OPERATOR(type)              \
    inline message_block & operator << (const type &v)  \
    {                                                   \
      if (this->space() >= sizeof(type))                \
      {                                                 \
        std::memcpy(this->wr_ptr(), &v, sizeof(type));  \
        this->wr_ptr(sizeof(type));                     \
      }                                                 \
      return *this;                                     \
    }

    MESSAGE_BLOCK_READ_OPERATOR(int8_t);
    MESSAGE_BLOCK_READ_OPERATOR(int16_t);
    MESSAGE_BLOCK_READ_OPERATOR(int32_t);
    MESSAGE_BLOCK_READ_OPERATOR(int64_t);

    MESSAGE_BLOCK_READ_OPERATOR(uint8_t);
    MESSAGE_BLOCK_READ_OPERATOR(uint16_t);
    MESSAGE_BLOCK_READ_OPERATOR(uint32_t);
    MESSAGE_BLOCK_READ_OPERATOR(uint64_t);

    MESSAGE_BLOCK_READ_OPERATOR(float);
    MESSAGE_BLOCK_READ_OPERATOR(double);

    MESSAGE_BLOCK_WRITE_OPERATOR(int8_t);
    MESSAGE_BLOCK_WRITE_OPERATOR(int16_t);
    MESSAGE_BLOCK_WRITE_OPERATOR(int32_t);
    MESSAGE_BLOCK_WRITE_OPERATOR(int64_t);

    MESSAGE_BLOCK_WRITE_OPERATOR(uint8_t);
    MESSAGE_BLOCK_WRITE_OPERATOR(uint16_t);
    MESSAGE_BLOCK_WRITE_OPERATOR(uint32_t);
    MESSAGE_BLOCK_WRITE_OPERATOR(uint64_t);

    MESSAGE_BLOCK_WRITE_OPERATOR(float);
    MESSAGE_BLOCK_WRITE_OPERATOR(double);
#undef MESSAGE_BLOCK_READ_OPERATOR
#undef MESSAGE_BLOCK_WRITE_OPERATOR
  protected:
    // point to beginning of message payload.
    bool        release_data_;
    char *	    mb_data_;
    size_t	    mb_size_;
    // message type
    int         mb_type_;
    // pointer to beginning of next read.
    size_t      rd_ptr_;
    // pointer to beginning of next write.
    size_t      wr_ptr_;

    // Pointer to previous message in the list.
    message_block *  prev_;
    // Pointer to next message in the list.
    message_block *  next_;
  private:
    // = Disallow these operations
    message_block &operator =(const message_block & );

    message_block(const message_block &);
  };
} // namespace ndk

#endif // NDK_MESSAGE_BLOCK_H_

