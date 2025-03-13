#include "reassembler.hh"

using namespace std;
//插入的过程中来做判断:能否补充到还未重组的流中
void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
    //最后一个字串处理完就关闭流
  if ( data.empty() ) {
    if ( is_last_substring ) {
      output.close();
    }
    return;
  }
  //没有可用空间直接退出
  if ( output.available_capacity() == 0 ) {
    return;
  }

  auto const end_index = first_index + data.size();
  auto const first_unacceptable = first_unassembled_index_ + output.available_capacity();

  // data is not in [first_unassembled_index, first_unacceptable] 不需要保存
  if ( end_index <= first_unassembled_index_ || first_index >= first_unacceptable ) {
    return;
  }

  // if part of data is out of capacity, then truncate it
  if ( end_index > first_unacceptable ) {
    data = data.substr( 0, first_unacceptable - first_index );
    // if truncated, it won't be last_substring
    is_last_substring = false;
  }

  // unordered bytes, save it in buffer and return 直接插入
  if ( first_index > first_unassembled_index_ ) {
    insert_into_buffer( first_index, std::move( data ), is_last_substring ); //插入之后就有序了
    return;
  }

  // remove useless prefix of data (i.e. bytes which are already assembled) 去除无效前缀
  if ( first_index < first_unassembled_index_ ) {
    data = data.substr( first_unassembled_index_ - first_index );
  }

  // here we have first_index == first_unassembled_index_
  first_unassembled_index_ += data.size(); 
  output.push( std::move( data ) );

  if ( is_last_substring ) {
    output.close();
  }
//这个时候需要删除buffer中已经从无序变为有序的部分
  if ( !buffer_.empty() && buffer_.begin()->first <= first_unassembled_index_ ) {
    pop_from_buffer( output );
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return buffer_size_;
}


//buffer中存储的虽然是未被重组的，但是是有序排放的
void Reassembler::insert_into_buffer( const uint64_t first_index, std::string&& data, const bool is_last_substring )
{
  auto begin_index = first_index;
  const auto end_index = first_index + data.size();
//保证有序 一定要遍历完整个新输入的子节
//[(3, "lo"), (20, "world")]中插入[(1, "hehehehehehehehehehehheheeheh")]
  for ( auto it = buffer_.begin(); it != buffer_.end() && begin_index < end_index; ) {
    if ( it->first <= begin_index ) {
      begin_index = max( begin_index, it->first + it->second.size() );
      ++it;
      continue;
    }
      //直接插入即可  例如[(3, "lo"), (20, "world")]中插入 (10, "www")刚好卡在中间
    if ( begin_index == first_index && end_index <= it->first ) {
      buffer_size_ += data.size();
      buffer_.emplace( it, first_index, std::move( data ) );
      return;
    }
//有重合 截取后半部分
    const auto right_index = min( it->first, end_index );
    const auto len = right_index - begin_index;
    buffer_.emplace( it, begin_index, data.substr( begin_index - first_index, len ) );
    buffer_size_ += len;
    begin_index = right_index;
  }
//这部分就是纯剩下来的因为此时it == buffer_.end()
  if ( begin_index < end_index ) {
    buffer_size_ += end_index - begin_index;
    buffer_.emplace_back( begin_index, data.substr( begin_index - first_index ) );
  }

  if ( is_last_substring ) {
    has_last_ = true;
  }
}
//用来将buffer中已经有序的放入output中去
void Reassembler::pop_from_buffer( Writer& output )
{
  for ( auto it = buffer_.begin(); it != buffer_.end(); ) {
    if ( it->first > first_unassembled_index_ ) {
      break;
    }
    // it->first <= first_unassembled_index_
    const auto end = it->first + it->second.size();
    if ( end <= first_unassembled_index_ ) {
      buffer_size_ -= it->second.size(); //这个时候已经被重组了 删去就行
    } else {
      auto data = std::move( it->second );
      buffer_size_ -= data.size();
      if ( it->first < first_unassembled_index_ ) {
        data = data.substr( first_unassembled_index_ - it->first );
      }
      first_unassembled_index_ += data.size();
      output.push( std::move( data ) ); 
    }
    it = buffer_.erase( it );
  }

  if ( buffer_.empty() && has_last_ ) {
    output.close();
  }
}