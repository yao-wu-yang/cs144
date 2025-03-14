#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
//���δʹ�ù̶��ĳ�ʼ���кţ�������һ����������к�
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms )
{} //optional�﷨

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
//��������message�Ļ���
// This is the TCPSender��s opportunity to actually send a TCPSenderMessage if it wants to
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
//�������������ѡ�����ݽ��з���
/*The TCPSender is asked to fill the window from the outbound byte stream: it reads
from the stream and generates as many TCPSenderMessages as possible, as long as
there are new bytes to be read and space available in the window.*/

void TCPSender::push( Reader& outbound_stream )
{
  // Your code here.
  (void)outbound_stream;
  size_t currwindow_size = window_size == 0 ? 1 : window_size; //����Ϊ0�Ļ�����Ϊ1
  while ( outstanding_cnt < currwindow_size ) { //ֻҪ���ڴ��䵫��δȷ�ϵ��ֽ�����outstanding_cnt��С�ڵ�ǰ���ڴ�С��currwindow_size�����ͼ����������ݡ�
    //�������ܷ�������·���Խ��Խ��
    
    TCPSenderMessage message;
    //�����һ�ν�������
    if ( !SYN_ ) {
      SYN_ = message.SYN = true; //��ʾ����SYN��
      outstanding_cnt++;
    }

    message.seqno = Wrap32::wrap( next_seqno, isn_ );
    auto const payload_size = min( TCPConfig::MAX_PAYLOAD_SIZE, currwindow_size - outstanding_cnt ); //

    //������������ж���payload_size��С�����ݲ��ͷ���Ӧ��С�Ļ������ռ�
    read( outbound_stream, payload_size, message.payload );
    outstanding_cnt += message.payload.size();
    //�����һ����ֹ����
    if ( !FIN_ && outbound_stream.is_finished() && outstanding_cnt < currwindow_size ) {
      FIN_ = message.FIN = true;
      outstanding_cnt++;
    }
    //�����ʱ������Ϊ��

    /*A segment that occupies no sequence numbers (no payload, SYN, or FIN) doesn��t need
     to be remembered or retransmitted.
    */
   //�����Ϣ�����кų���Ϊ 0����û����Ч�غɡ�SYN �� FIN�������˳�ѭ����������Ϣ����Ҫ����ס���ش���
    if ( message.sequence_length() == 0 ) {
      break;
    }

    queue_segments.push_back( message );
    next_seqno += message.sequence_length();
    outstanding_segments.push_back( message );//����һ�¸���
    //��ʱ��ֹ���Ӳ������������Ϊ��
    if ( message.FIN && outbound_stream.bytes_buffered() == 0 ) {
      break;
    }
  }
}

TCPSenderMessage TCPSender::send_empty_message() const // ����̽����ܷ��Ĵ���״̬���߱������ӻ�Ծ
{
  // Your code here.
  auto seqno = Wrap32::wrap( next_seqno, isn_ );
  return { seqno, false, {}, false };
}
//ֻ�����յ�ACK��ʱ��ſ���������ͷ�����������ݱ�����
void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  (void)msg;
  window_size = msg.window_size;
  if ( msg.ackno.has_value() ) {
    auto ackno = msg.ackno.value().unwrap( isn_, next_seqno );
    //��Ч��ACK �����˽�Ҫ���͵���һ���ֽں�(impossible)
    if ( ackno > next_seqno ) {
      return;
    }

    ack_seqno = ackno;

    while ( !outstanding_segments.empty() ) {
      //ɾ�������δ�ɹ����ܵı��ģ���֤outstanding_segments�д洢��ʼ�ն���δ��ȷ�ϵı���
      auto& temp = outstanding_segments.front();
      if ( temp.seqno.unwrap( isn_, next_seqno ) + temp.sequence_length() <= ack_seqno ) {
        outstanding_cnt -= temp.sequence_length();
        outstanding_segments.pop_front();
        timer_.reset_RTO(); //���¼�ʱ������Ϊ��Щ���ݰ��Ѿ��������ˣ�����Ҫ�ش���ʱ��
        if ( !outstanding_segments.empty() ) {
          timer_.start();
        }
        retransmit_cnt = 0;
      } else {
        break;
      }
    }
    //�����Ҫ�ش��Ķ��п�
    if ( outstanding_segments.empty() ) {
      timer_.stop();
    }
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  (void)ms_since_last_tick;
  timer_.tick( ms_since_last_tick ); //�����ms_since_last_tickӦ������ǰ���ݹ�ʽ������
  if ( timer_.is_expire() ) {
    //��ʱ��Ҫ�ش���С��ŵı���
    queue_segments.push_back( outstanding_segments.front() ); //�����ش����糬ʱ�ı���
    if ( window_size != 0 ) {
      ++retransmit_cnt;
      timer_.double_RTO();
    }
    timer_.start();
  }
}

