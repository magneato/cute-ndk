// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2009-05-29 02:24
 */
//========================================================================

#ifndef NDK_EVENT_HANDLER_H_
#define NDK_EVENT_HANDLER_H_

#include "ndk/types.h"

namespace ndk
{
  // Forward declarations
  // 
  class reactor; 
  class time_value;
  
  typedef int reactor_mask;

  /**
   * @class event_handler
   *
   * @brief 
   */
  class event_handler
  {
  public:
    enum 
    {
      null_mask       = 0,
      read_mask       = 1L << 1,
      write_mask      = 1L << 2,
      except_mask     = 1L << 3,
      accept_mask     = 1L << 4,
      connect_mask    = 1L << 5,
      timer_mask      = 1L << 6,
      epoll_edge      = 1L << 7,
      dont_call       = 1L << 31,

      all_events_mask = read_mask  | \
                        write_mask   |
                        except_mask  |
                        accept_mask  |
                        connect_mask |
                        timer_mask   |
                        epoll_edge   ,
    };
  public:
    virtual ~event_handler() {}
    
    // The handle_close(ndk_handle, reactor_mask) will be called if return -1. 
    inline virtual int handle_input(ndk_handle /*handle*/)
    { return -1; }
    
    // The handle_close(ndk_handle, reactor_mask) will be called if return -1. 
    inline virtual int handle_output(ndk_handle /*handle*/)
    { return -1; }

    // The handle_close(ndk_handle, reactor_mask) will be called if return -1. 
    inline virtual int handle_exception(ndk_handle /*handle*/)
    { return -1; }

    // The handle_close(const void *, reactor_mask) will be called if return -1.
    inline virtual int handle_timeout(const void * /*arg*/, 
                                      const time_value & /*curent_time*/)
    { return -1; }

    // For notify mechanism.
    // The handle_close(const void *, reactor_mask) will be called if return -1.
    inline virtual int handle_msg(void * /*msg*/)
    { return -1; }

    //
    inline virtual int handle_close(ndk_handle /*handle*/, reactor_mask /*mask*/)
    { return -1; }

    // Get the I/O handle
    inline virtual ndk_handle get_handle() const
    {
      return NDK_INVALID_HANDLE;
    }

    // Set the I/O handle
    inline virtual void set_handle(ndk_handle /*handle*/)
    { }

    // Get the event demultiplexors.
    inline virtual void set_reactor(reactor *r) 
    { this->reactor_ = r; }

    // Set the event demultiplexors.
    inline virtual reactor *get_reactor(void) const
    { return this->reactor_; }
  protected:
    // Force event_handler to be an abstract base class.
    event_handler()
      : reactor_(0)
    { }

    reactor *reactor_;
  };
} // namespace ndk

#endif // NDK_EVENT_HANDLER_H_

