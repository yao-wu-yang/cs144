#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  // Your code here.
  (void)message;
  (void)reassembler;
  (void)inbound_stream;
  if(!_syn&&message.SYN){
    _isn=message.seqno;
    _syn=true;
  }
  if(!_syn)return;
  std::string payload=message.payload;
   Wrap32 seqno=message.SYN?message.seqno+1:message.seqno;
   uint64_t checkpoint=inbound_stream.bytes_pushed();
   uint64_t index=seqno.unwrap(_isn,checkpoint)-1;
   reassembler.insert(index,payload,_syn&&message.FIN,inbound_stream);
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  // Your code here.
 (void)inbound_stream;
    TCPReceiverMessage temp;
    if(!_syn){
      temp.ackno={};
      if(inbound_stream.writer().available_capacity()>=65535)temp.window_size=65535;
      else temp.window_size=inbound_stream.writer().available_capacity();
      return temp;
    }
    else {
      size_t ack=inbound_stream.writer().bytes_pushed()+1;
      if(inbound_stream.writer()._input_ended_flag){
        temp.ackno=_isn.wrap(ack+1,_isn);
        if(inbound_stream.writer().available_capacity()>=65535)temp.window_size=65535;
      else temp.window_size=inbound_stream.writer().available_capacity();
      return temp;
      }
      else {
        temp.ackno=_isn.wrap(ack,_isn);
        if(inbound_stream.writer().available_capacity()>=65535)temp.window_size=65535;
      else temp.window_size=inbound_stream.writer().available_capacity();
      return temp;
      }
    }



}
