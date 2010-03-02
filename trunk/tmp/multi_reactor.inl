namespace ndk
{
inline
template<typename REACTOR>
int multi_reactor_handle_repository<REACTOR>::is_invalid_handle(ndk_handle handle)
{
  if (handle == NDK_INVALID_HANDLE
#if defined(NDK_LINUX)
      && handle >= this->max_size_
#endif
      )
    return 1;
  return 0;
}
inline
template<typename REACTOR>
int multi_reactor_handle_repository<REACTOR>::handle_in_range(ndk_handle handle)
{
  if (handle >= NDK_INVALID_HANDLE
#if defined(NDK_LINUX)
      && handle < this->max_size_
#endif
      )
    return 1;
  return 0;
}
inline
template<typename REACTOR>
int multi_reactor_handle_repository<REACTOR>::mask(ndk_handle handle)
{
  if (this->handle_in_range(handle))
    return this->reactor_handle_map_[handle].mask_;
  return event_handler::null_mask;
}
inline
template<typename REACTOR>
void multi_reactor_handle_repository<REACTOR>::mask(ndk_handle handle,
                                                   reactor_mask mask)
{
  if (this->handle_in_range(handle))
    this->reactor_handle_map_[handle].mask_ = mask;
}
inline
template<typename REACTOR>
REACTOR *multi_reactor_handle_repository<REACTOR>::find(ndk_handle handle)
{
#if defined(NDK_WIN32)
  reactor_handle_map_iter iter = this->reactor_handle_map_.find(handle);
  if (iter != this->reactor_handle_map_.end())
    return iter->second;
#else
  REACTOR *r = 0;
  if (this->handle_in_range(handle))
  {
    r = this->reactor_handle_map_[handle];
    return r;
  }
#endif
  return 0;
}
inline 
template<typename REACTOR>
int multi_reactor_handle_repository<REACTOR>::bind(ndk_handle handle, 
                                                   REACTOR *r,
                                                   reactor_mask mask)
{
  if (this->is_invalid_handle(handle))
    return -1;
#if defined(NDK_WIN32)
  reactor_handle_map_iter iter = this->reactor_handle_map_.find(handle);
  if (iter != this->reactor_handle_map_.end())
    return -1;
  this->reactor_handle_map_.insert(std::make_pair(handle, r));
#else
  this->reactor_handle_map_[handle].reactor_ = r;
  this->reactor_handle_map_[handle].mask_ = mask;
#endif
  return 0;
}
inline
template<typename REACTOR>
int multi_reactor_handle_repository<REACTOR>::unbind(ndk_handle handle)
{
  if (this->find(handle) == 0)
    return -1;
#if defined(NDK_WIN32)  
  this->reactor_handle_map_.erase(handle);
#else
  this->reactor_handle_map_[handle] = 0;
#endif
  return 0;
}
} // namespace ndk
