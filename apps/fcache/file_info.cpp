#include "file_info.h"
#include "pipe_buffer.h"

void file_info::insert_pipe_buffer(pipe_buffer *pbuffer)
{
  ndk::guard<ndk::thread_mutex> g(this->pipe_buffers_mtx_);

  assert(pbuffer->chunk_id() >= 0 && pbuffer->chunk_id() <= this->length_/PIPE_CHUNK_SIZE);
  assert(this->pipe_buffers_[pbuffer->chunk_id()] == 0);

  this->pipe_buffers_[pbuffer->chunk_id()] = pbuffer;
  return ;
}
pipe_buffer *file_info::find_pipe_buffer(int chunk_id)
{
  ndk::guard<ndk::thread_mutex> g(this->pipe_buffers_mtx_);
  assert(chunk_id >= 0 && chunk_id <= this->length_/PIPE_CHUNK_SIZE);

  return this->pipe_buffers_[chunk_id];
}
pipe_buffer *file_info::release_pipe_buffer(int chunk_id)
{
  ndk::guard<ndk::thread_mutex> g(this->pipe_buffers_mtx_);
  assert(chunk_id >= 0 && chunk_id <= this->length_/PIPE_CHUNK_SIZE);
  pipe_buffer *pbuffer = this->pipe_buffers_[chunk_id];
  this->pipe_buffers_[chunk_id] = 0;
  return pbuffer;
}
void file_info::set_bitmap(int chunk_id)
{
  ndk::guard<ndk::thread_mutex> g(this->bitmap_mtx_);
  assert(chunk_id >= 0 && chunk_id <= this->length_/PIPE_CHUNK_SIZE);
  this->bitmap_[chunk_id] = '1';
}
bool file_info::check_file_completed()
{
  ndk::guard<ndk::thread_mutex> g(this->bitmap_mtx_);
  int chunks = this->length_/PIPE_CHUNK_SIZE + 1;
  for (int i = 0; i < chunks; ++i)
  {
    if (this->bitmap_[i] == '0')
      return false;
  }
  return true;
}
