#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
//如果未使用固定的初始序列号，就生成一个随机的序列号
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms )
{}

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
//This is the TCPSender’s opportunity to actually send a TCPSenderMessage if it wants to
optional<TCPSenderMessage> TCPSender::maybe_send()
{
  // Your code here.
  if(queue_segments.empty()){
    return {};
  }
  if(!timer_.is_run()){
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
  size_t currwindow_size=window_size==0?1:window_size;
  while (outstanding_cnt<currwindow_size)
  {
    TCPSenderMessage message;
  //如果第一次建立连接
    if(!SYN_){
      SYN_=message.SYN=true;
      outstanding_cnt++;
    }
    
    message.seqno=Wrap32::wrap(next_seqno,isn_);
    auto const payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE,currwindow_size-outstanding_cnt);
    //从输出缓冲区中读出payload_size大小的数据并释放相应大小的缓冲区空间
    read(outbound_stream,payload_size,message.payload);
    outstanding_cnt+=message.payload.size();
    //如果第一次终止连接
    if(!FIN_&&outbound_stream.is_finished()&&outstanding_cnt<currwindow_size){
      FIN_=message.FIN=true;
      outstanding_cnt++;
    }
    //如果此时缓冲区为空
    
    /*A segment that occupies no sequence numbers (no payload, SYN, or FIN) doesn’t need
     to be remembered or retransmitted.
    */
    if(message.sequence_length()==0){  
      break;
    }
    
    queue_segments.push_back(message);
    next_seqno+=message.sequence_length();
    outstanding_segments.push_back(message);
    //此时终止连接并且输出缓冲区为空
    if(message.FIN&&outbound_stream.bytes_buffered()==0){
      break;
    }
  }
  
  
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // Your code here.
  auto seqno=Wrap32::wrap(next_seqno,isn_);
  return {seqno,false,{},false};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  (void)msg;
  window_size=msg.window_size;
  if(msg.ackno.has_value()){
    auto ackno=msg.ackno.value().unwrap(isn_,next_seqno);
    //收到的ACK大于期望的 说明已经成功接受数据 不需要重传
    if(ackno>next_seqno){
      return ;
    }

    ack_seqno=ackno;

    while(!outstanding_segments.empty()){
      //删除最早的未成功接受的报文，保证outstanding_segments中存储的始终都是未被确认的报文
      auto& temp=outstanding_segments.front();
      if(temp.seqno.unwrap(isn_,next_seqno)+temp.sequence_length()<=ack_seqno){
        outstanding_cnt-=temp.sequence_length();
        outstanding_segments.pop_front();
        timer_.reset_RTO();
        if(!outstanding_segments.empty()){
          timer_.start();
        }
        retransmit_cnt=0;
      }
      else {
        break;
      }
    }
    //如果需要重传的队列空
    if(outstanding_segments.empty()){
      timer_.stop();
    }
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  (void)ms_since_last_tick;
   timer_.tick( ms_since_last_tick );
  if ( timer_.is_expire() ) {
    //此时需要重传最小序号的报文
    queue_segments.push_back( outstanding_segments.front() );
    if ( window_size != 0 ) {
      ++retransmit_cnt;
      timer_.double_RTO();
    }
    timer_.start();
  }
}
