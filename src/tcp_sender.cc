#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
//如果未使用固定的初始序列号，就生成一个随机的序列号
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms )
{} //optional语法

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return outstanding_cnt;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return retransmit_cnt;
}
//真正发送message的机会
// This is the TCPSender’s opportunity to actually send a TCPSenderMessage if it wants to
optional<TCPSenderMessage> TCPSender::maybe_send()
{
  // Your code here.
  if ( queue_segments.empty() ) {
    return {};
  }
  if ( !timer_.is_run() ) {
    timer_.start();
  }
  auto message = queue_segments.front();
  queue_segments.pop_front();
  return message;
}
//从输出缓冲区中选择数据进行发送
/*The TCPSender is asked to fill the window from the outbound byte stream: it reads
from the stream and generates as many TCPSenderMessages as possible, as long as
there are new bytes to be read and space available in the window.*/

void TCPSender::push( Reader& outbound_stream )
{
  // Your code here.
  (void)outbound_stream;
  size_t currwindow_size = window_size == 0 ? 1 : window_size; //窗口为0的话调整为1
  while ( outstanding_cnt < currwindow_size ) { //只要正在传输但尚未确认的字节数（outstanding_cnt）小于当前窗口大小（currwindow_size），就继续发送数据。
    //有数据能发的情况下发的越多越好
    
    TCPSenderMessage message;
    //如果第一次建立连接
    if ( !SYN_ ) {
      SYN_ = message.SYN = true; //表示发送SYN包
      outstanding_cnt++;
    }

    message.seqno = Wrap32::wrap( next_seqno, isn_ );
    auto const payload_size = min( TCPConfig::MAX_PAYLOAD_SIZE, currwindow_size - outstanding_cnt ); //

    //从输出缓冲区中读出payload_size大小的数据并释放相应大小的缓冲区空间
    read( outbound_stream, payload_size, message.payload );
    outstanding_cnt += message.payload.size();
    //如果第一次终止连接
    if ( !FIN_ && outbound_stream.is_finished() && outstanding_cnt < currwindow_size ) {
      FIN_ = message.FIN = true;
      outstanding_cnt++;
    }
    //如果此时缓冲区为空

    /*A segment that occupies no sequence numbers (no payload, SYN, or FIN) doesn’t need
     to be remembered or retransmitted.
    */
   //如果消息的序列号长度为 0（即没有有效载荷、SYN 或 FIN），则退出循环。这种消息不需要被记住或重传。
    if ( message.sequence_length() == 0 ) {
      break;
    }

    queue_segments.push_back( message );
    next_seqno += message.sequence_length();
    outstanding_segments.push_back( message );//保存一下副本
    //此时终止连接并且输出缓冲区为空
    if ( message.FIN && outbound_stream.bytes_buffered() == 0 ) {
      break;
    }
  }
}

TCPSenderMessage TCPSender::send_empty_message() const // 用于探测接受方的窗口状态或者保持连接活跃
{
  // Your code here.
  auto seqno = Wrap32::wrap( next_seqno, isn_ );
  return { seqno, false, {}, false };
}
//只有在收到ACK的时候才考虑清除发送方所保存的数据报副本
void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  (void)msg;
  window_size = msg.window_size;
  if ( msg.ackno.has_value() ) {
    auto ackno = msg.ackno.value().unwrap( isn_, next_seqno );
    //无效的ACK 超过了将要发送的下一个字节号(impossible)
    if ( ackno > next_seqno ) {
      return;
    }

    ack_seqno = ackno;

    while ( !outstanding_segments.empty() ) {
      //删除最早的未成功接受的报文，保证outstanding_segments中存储的始终都是未被确认的报文
      auto& temp = outstanding_segments.front();
      if ( temp.seqno.unwrap( isn_, next_seqno ) + temp.sequence_length() <= ack_seqno ) {
        outstanding_cnt -= temp.sequence_length();
        outstanding_segments.pop_front();
        timer_.reset_RTO(); //更新计时器，因为这些数据包已经被接受了，不需要重传计时了
        if ( !outstanding_segments.empty() ) {
          timer_.start();
        }
        retransmit_cnt = 0;
      } else {
        break;
      }
    }
    //如果需要重传的队列空
    if ( outstanding_segments.empty() ) {
      timer_.stop();
    }
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  (void)ms_since_last_tick;
  timer_.tick( ms_since_last_tick ); //这里的ms_since_last_tick应该是提前根据公式计算了
  if ( timer_.is_expire() ) {
    //此时需要重传最小序号的报文
    queue_segments.push_back( outstanding_segments.front() ); //优先重传最早超时的报文
    if ( window_size != 0 ) {
      ++retransmit_cnt;
      timer_.double_RTO();
    }
    timer_.start();
  }
}

