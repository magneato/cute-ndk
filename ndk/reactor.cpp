#include "ndk/reactor.h"
#include "ndk/epoll_reactor.h"

namespace ndk
{
reactor *reactor::reactor_ = 0;
thread_mutex reactor::instance_lock_;

reactor::reactor(reactor_impl *impl/* = 0*/,
                 int delete_impl/* = 0*/)
  : reactor_impl_(impl),
  delete_impl_(delete_impl)
{
  if (this->reactor_impl_ == 0)
  {
    /**
     * epoll reactor is default reactor in linux platfrom.
     */
    this->reactor_impl_ = new epoll_reactor<reactor_null_token>();
    this->delete_impl_  = 1;
  }
}
reactor::~reactor()
{
  if (this->reactor_impl_ && this->delete_impl_)
    delete this->reactor_impl_;
}
} // namespace ndk
