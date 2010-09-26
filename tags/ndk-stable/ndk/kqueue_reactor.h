// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw <shaovie@gmail.com>
 * Date     : 2010-02-20 21:39
 */
//========================================================================

#ifndef NDK_KQUEUE_REACTOR_H_
#define NDK_KQUEUE_REACTOR_H_

#include "ndk/config.h"
#include "ndk/strace.h"
#include "ndk/unix_reactor.h"

namespace ndk
{
  /**
   * @class kqueue_reactor_base
   *
   */
  class kqueue_reactor_base : public unix_reactor
  {
  protected:
    kqueue_reactor_base();

    //
    int open_i(timer_queue *tq);

    //
    int close_i(void);

    virtual int poll_events(const time_value *);

    //
    int dispatch_io_events();

    //
    int reactor_mask_to_kqueue_event(reactor_mask mask);

    //
    int handle_opt_i(ndk_handle handle, reactor_mask mask, int opt);
  protected:
    bool initialized_;

    // The file descriptor associated with the open `/dev/kqueue' device.
    int kqueue_fd_;

    int nevents_;

    // Table of event structures to be filled by kevent:
    struct kqueue_event *events_;

    // Pointer to the kqueue_event array element that contains the 
    // event to be dispatched.
    struct kqueue_event *changed_events_;
  };
  /**
   * @class kqueue_reactor
   * 
   * @brief
   */
  class kqueue_reactor
  {
  public:
    kqueue_reactor()
    {
    }

    virtual ~kqueue_reactor()
    {
    }
  };
} // namespace ndk

#endif // NDK_KQUEUE_REACTOR_H_

