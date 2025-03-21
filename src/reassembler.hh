#pragma once

#include "byte_stream.hh"

#include <string>

class Reassembler
{
private:
  uint64_t first_unassembled_index_ { 0 };

  std::list<std::pair<uint64_t, std::string>> buffer_ {}; //用来保存失序的字节,不能包含重叠数据，相当于接受方使用TCP协议存储了失序的包
  uint64_t buffer_size_ { 0 };
  bool has_last_ { false };

  // insert valid but un-ordered data into buffer
  void insert_into_buffer( uint64_t first_index, std::string&& data, bool is_last_substring );

  // pop invalid bytes and insert valid bytes into writer !!!
  void pop_from_buffer( Writer& output );

public:
  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *   `output`: a mutable reference to the Writer //接受方一侧的进程从这里提取相应的数据
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
   * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
   * learns the next byte in the stream, it should write it to the output.
   *
   * If the Reassembler learns about bytes that fit within the stream's available capacity
   * but can't yet be written (because earlier bytes remain unknown), it should store them
   * internally until the gaps are filled in.
   *
   * The Reassembler should discard any bytes that lie beyond the stream's available capacity
   * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
   *
   * The Reassembler should close the stream after writing the last byte.
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring, Writer& output );

  // How many bytes are stored in the Reassembler itself?
  uint64_t bytes_pending() const;
};
