#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
//计时器
class Timer
{
protected:
  uint64_t Initial_RTO_ms_; //声明一个初始的RTO值
  uint64_t current_RTO_ms_; //声明一个当前的RTO值
  size_t time_ms { 0 };     //表示已经经过的时间
  bool running_ { false };  //标识计时器是否运行
public:
  explicit Timer( uint64_t init_RTO_ ) : Initial_RTO_ms_( init_RTO_ ), current_RTO_ms_( init_RTO_ ) {}
  void start()
  {
    running_ = true;
    time_ms = 0;
  }

  void stop() { running_ = false; }

  bool is_run() const { return running_; }

  bool is_expire() const { return running_ && ( time_ms >= current_RTO_ms_ ); }

  void tick( size_t const ms_since_last_tick )
  {
    if ( running_ ) {
      time_ms += ms_since_last_tick;
    }
  }

  void double_RTO() { current_RTO_ms_ *= 2; } //

  void reset_RTO() { current_RTO_ms_ = Initial_RTO_ms_; }
};

class TCPSender
{
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  bool SYN_ { false };
  bool FIN_ { false };
  unsigned retransmit_cnt { 0 }; //记录连续重传的次数 ，用于实现TCP的指数退避策略

  uint64_t ack_seqno { 0 };   //接受方已成功收到的数据的序列号
  uint64_t next_seqno { 0 };  //下一个要发送的数据的序列号 这里不同于TCP协议中的32位固定序列号
  uint16_t window_size { 1 };  //

  uint64_t outstanding_cnt { 0 }; //记录正在传输的字节数(未被确认)
  std::deque<TCPSenderMessage> outstanding_segments {}; //保存发送方所发出的但还未被确认的数据包
  std::deque<TCPSenderMessage> queue_segments {};

  Timer timer_ { initial_RTO_ms_ };

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream ); //用于读取待发送的数据。

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send(); //如果需要发送数据，则返回一个 TCPSenderMessage，否则返回空

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg ); //接收并处理来自接收方的 TCPReceiverMessage。

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick ); //更新时间，并检查是否需要重传数据

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding? 返回正在传输但尚未确认的字节数。
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened? 返回连续重传的次数。
};
