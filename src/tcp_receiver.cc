#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  //先判断是不是在建立链接的情况下
  if ( !isn_.has_value() ) {
    if ( !message.SYN ) { //若不是SYN包 也就是三次握手的第一个报文段
      return;
    }
    isn_ = message.seqno; //此时表示建立链接的第一个报文
  }
  /*
   * to get first_index (stream index), we need to
   * 1. convert message.seqno to absolute seqno
   * 2. convert absolute seqno to stream index
   */

  // 1. convert message.seqno to absolute seqno
  // checkpoint, i.e. first unassembled index (stream index)
  auto const checkpoint = inbound_stream.bytes_pushed() + 1; // + 1: stream index to absolute seqno
  auto const abs_seqno = message.seqno.unwrap( isn_.value(), checkpoint );

  // 2. convert absolute seqno to stream index
  //网络上的32位要进行转换
  auto const first_index = message.SYN ? 0 : abs_seqno - 1;
  reassembler.insert( first_index, message.payload.release(), message.FIN, inbound_stream );
   // std::string&& release() { return std::move( *buffer_ ); }
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  TCPReceiverMessage msg {};
  auto const win_sz = inbound_stream.available_capacity(); //接受窗口的大小
  msg.window_size = win_sz < UINT16_MAX ? win_sz : UINT16_MAX; //TCP中窗口大小2字节16位

  if ( isn_.has_value() ) { //初始序列号一直存在
    // convert from stream index to abs seqno
    // + 1 是因为ACK是下一个字节序号 + inbound_stream.is_closed() for FIN
    uint64_t const abs_seqno = inbound_stream.bytes_pushed() + 1 + inbound_stream.is_closed(); //SYN和FIN都要消耗一个序号
    msg.ackno = Wrap32::wrap( abs_seqno, isn_.value() );
  }
  return msg;
}
