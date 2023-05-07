#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ){}

void Writer::push( string data )
{
  // Your code here.
  (void)data;
  if(_input_ended_flag)return;
  uint64_t len=data.size();
  uint64_t minlen=min(len,available_capacity());
  _write_count+=minlen;
  for(uint64_t i=0;i<minlen;i++)_buffer.push_back(data[i]);
  }

void Writer::close()
{
  // Your code here.
  _input_ended_flag=true;
}

void Writer::set_error()
{
  // Your code here.
  _error=true;
}

bool Writer::is_closed() const
{
  // Your code here.
  return _input_ended_flag;
  
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
   return capacity_-_buffer.size();
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return _write_count;
}

string Reader::peek() const
{
  // Your code here.
  size_t len=_buffer.size();
  string temp;
  temp=std::move(_buffer.substr(0,len));
  return temp;
}

bool Reader::is_finished() const
{
  // Your code here.
  return _input_ended_flag&&_buffer.empty();
}

bool Reader::has_error() const
{
  // Your code here.
  return _error;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  uint64_t minlen=min(len,_buffer.size());
  _read_count+=len;
  //for(uint64_t i=0;i<minlen;i++)_buffer
  _buffer=std::move(_buffer.substr(minlen));
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return _buffer.size();
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return _read_count;
}
