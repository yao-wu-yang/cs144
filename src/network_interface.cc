#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  (void)dgram;
  (void)next_hop;
  //�õ�Ŀ��ip
  auto const& target_ip = next_hop.ipv4_numeric();
  //�Ȳ�ѯARP��
  if ( ip2ether_.count( target_ip ) ) {
    EthernetFrame frame { { ip2ether_[target_ip].first, ethernet_address_, EthernetHeader::TYPE_IPv4 }, //Ŀ�ĵ�ַ����ʼ��ַ
                          serialize( dgram ) }; //������̫��֡
    out_frames.push( std::move( frame ) );
  }
  /*If the destination Ethernet address is unknown, broadcast an ARP request for the
next hop��s Ethernet address, and queue the IP datagram so it can be sent after
the ARP reply is received.*/
  else { //�����ʱ��ARP����û���ҵ���Ӧ�ı���
    //��һֱ��ARP request You don��t want to flood the network with ARP requests.
    if ( !arp_timer.count( target_ip ) ) {  //����Ƿ��Ѿ�������ARP���� (��ʵ���Բ�ʹ��ʱ��� ֱ��++����) ����ֻ++ ��Ϊ���������ʱ��Ļ� ��ʱ����ЩARP�����Ŀ�����Զ���ᵽ��
      ARPMessage request_message;
      request_message.opcode = ARPMessage::OPCODE_REQUEST; //����������ΪARP����Ķ���
      request_message.sender_ethernet_address = ethernet_address_; //��ARP�����ĵ�Դ��ַ�Ǹ�����ӿڵĵ�ַ
      request_message.sender_ip_address = ip_address_.ipv4_numeric();//IP��ַ������ӿڵ�IP��ַ
      request_message.target_ip_address = target_ip;//Ŀ�ĵ�ַ
      EthernetFrame frame { { ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP },
                            serialize( request_message ) }; //����㲥֡
      out_frames.push( std::move( frame ) );
      arp_timer.emplace( next_hop.ipv4_numeric(), 0 ); //���͵ĳ�ʼʱ��Ϊ0
      wait_drgams.insert( { target_ip, { dgram } } ); //�ȴ������в�������͵�������ݱ�
    } else {
      wait_drgams[target_ip].push_back( dgram ); //��� arp_timer ���Ѵ���Ŀ�� IP ��ַ�ļ�¼����ʾ�Ѿ����͹� ARP ���󣬴�ʱֻ�轫 IP ���ݱ�����ȴ�����
    }
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  (void)frame;
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) { //�յ�������֡��Ŀ�ĵ�ַ�Ȳ��Ǳ�����Ҳ���ǹ㲥֡ ֱ�Ӷ��� ��Ϊ�㲥֡��������
    return {};
  }
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) { //IPV4�����ݱ�ֱ����ȡ����
    InternetDatagram dgram;
    if ( parse( dgram, frame.payload ) ) {
      return dgram; //��������ݱ����ظ�������caller
    }
  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage message;

    if ( parse( message, frame.payload ) ) { //��ȡmessage�ɹ�
      ip2ether_.insert( { message.sender_ip_address, { message.sender_ethernet_address, 0 } } ); //ARP����ñ����ʱ��Ϊ0
      //�յ�ARP request
      if ( message.opcode == ARPMessage::OPCODE_REQUEST ) {//����յ���ARP����
        if ( message.target_ip_address == ip_address_.ipv4_numeric() ) {  //ȷ���Ƕ��Լ�������
          ARPMessage reply;
          reply.opcode = ARPMessage::OPCODE_REPLY;
          reply.sender_ethernet_address = ethernet_address_;
          reply.sender_ip_address = ip_address_.ipv4_numeric();
          reply.target_ethernet_address = message.sender_ethernet_address;
          reply.target_ip_address = message.sender_ip_address;
          EthernetFrame reply_frame {
            { message.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_ARP }, serialize( reply ) };
          out_frames.push( std::move( reply_frame ) ); //׼�����͸������� ��������Ϊip��ַ�ǹ̶�����
        }
      }
      //�յ�ARP reply
      else if ( message.opcode == ARPMessage::OPCODE_REPLY ) {
        //����ARP��
        ip2ether_.insert( { message.sender_ip_address, { message.sender_ethernet_address, 0 } } );
        auto const& dgram = wait_drgams[message.sender_ip_address]; //��ȡ�����͵�����
        for ( auto const& dgrams : dgram ) {
          send_datagram( dgrams, Address::from_ipv4_numeric( message.sender_ip_address ) );
        }
        wait_drgams.erase( message.sender_ip_address );
      }
    }

  }

  return {};
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
//����ARP���ÿ��Ŀ��ip��arp_timer
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  (void)ms_since_last_tick;
  size_t IP_MAP_TTL = 30000;
  size_t ARP_request_TTL = 5000; 

  for ( auto it = ip2ether_.begin(); it != ip2ether_.end(); ) {
    it->second.second += ms_since_last_tick;
    if ( it->second.second >= IP_MAP_TTL ) { //ARP�����ʱ����Ҫɾ��
      it = ip2ether_.erase( it );
    } else {
      it++;
    }
  }

  for ( auto it = arp_timer.begin(); it != arp_timer.end(); ) {
    it->second += ms_since_last_tick;
    if ( it->second >= ARP_request_TTL ) {
      it = arp_timer.erase( it );
    } else {
      it++;
    }
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if ( out_frames.empty() ) {
    return {};
  }
  auto frame = out_frames.front();
  out_frames.pop();
  return frame;
}

//����ӿڵ����� ������������·��� "����֡"   ������������  "���ݰ�"
//�������ݰ���ʱ���Ȳ���ARP���棬���û���ҵ��Ļ����Ȳ�ѯ�Ƿ��Ѿ����͹�ARP�����ˣ������û�з��͵Ļ��͹���㲥֡ARP������
//�յ�����֡��ʱ�����ж�MAC��ַ�ǲ��Ǳ��ӿڵ�ַ/�㲥��ַ�������IPV4�� ֱ�ӷ��ظ������� �����������ȷ���Ƕ��Լ������󷵻ع���ظ�֡������ǻظ����ģ���ô�����ӵ�ARP������ȥ��Ȼ�����
//Ŀ��IP��ֱַ�ӷ��͸��Է�