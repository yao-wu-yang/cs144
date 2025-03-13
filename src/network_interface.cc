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
  //得到目的ip
  auto const& target_ip = next_hop.ipv4_numeric();
  //先查询ARP表
  if ( ip2ether_.count( target_ip ) ) {
    EthernetFrame frame { { ip2ether_[target_ip].first, ethernet_address_, EthernetHeader::TYPE_IPv4 }, //目的地址和起始地址
                          serialize( dgram ) }; //构建以太网帧
    out_frames.push( std::move( frame ) );
  }
  /*If the destination Ethernet address is unknown, broadcast an ARP request for the
next hop’s Ethernet address, and queue the IP datagram so it can be sent after
the ARP reply is received.*/
  else { //如果此时在ARP表中没有找到对应的表项
    //别一直发ARP request You don’t want to flood the network with ARP requests.
    if ( !arp_timer.count( target_ip ) ) {  //检查是否已经发送了ARP请求 (其实可以不使用时间的 直接++？？) 不能只++ 因为如果不设置时间的话 超时的那些ARP请求报文可能永远不会到达
      ARPMessage request_message;
      request_message.opcode = ARPMessage::OPCODE_REQUEST; //创建操作码为ARP请求的对象
      request_message.sender_ethernet_address = ethernet_address_; //该ARP请求报文的源地址是该网络接口的地址
      request_message.sender_ip_address = ip_address_.ipv4_numeric();//IP地址是网络接口的IP地址
      request_message.target_ip_address = target_ip;//目的地址
      EthernetFrame frame { { ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP },
                            serialize( request_message ) }; //构造广播帧
      out_frames.push( std::move( frame ) );
      arp_timer.emplace( next_hop.ipv4_numeric(), 0 ); //发送的初始时间为0
      wait_drgams.insert( { target_ip, { dgram } } ); //等待队列中插入待发送的这个数据报
    } else {
      wait_drgams[target_ip].push_back( dgram ); //如果 arp_timer 中已存在目标 IP 地址的记录，表示已经发送过 ARP 请求，此时只需将 IP 数据报放入等待队列
    }
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  (void)frame;
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) { //收到的网络帧的目的地址既不是本网络也不是广播帧 直接丢弃 因为广播帧无条接收
    return {};
  }
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) { //IPV4的数据报直接提取就行
    InternetDatagram dgram;
    if ( parse( dgram, frame.payload ) ) {
      return dgram; //将这个数据报返回给调用者caller
    }
  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage message;

    if ( parse( message, frame.payload ) ) { //提取message成功
      ip2ether_.insert( { message.sender_ip_address, { message.sender_ethernet_address, 0 } } ); //ARP缓存该表项的时间为0
      //收到ARP request
      if ( message.opcode == ARPMessage::OPCODE_REQUEST ) {//如果收到是ARP请求
        if ( message.target_ip_address == ip_address_.ipv4_numeric() ) {  //确保是对自己的请求
          ARPMessage reply;
          reply.opcode = ARPMessage::OPCODE_REPLY;
          reply.sender_ethernet_address = ethernet_address_;
          reply.sender_ip_address = ip_address_.ipv4_numeric();
          reply.target_ethernet_address = message.sender_ethernet_address;
          reply.target_ip_address = message.sender_ip_address;
          EthernetFrame reply_frame {
            { message.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_ARP }, serialize( reply ) };
          out_frames.push( std::move( reply_frame ) ); //准备发送给请求者 单播，因为ip地址是固定的了
        }
      }
      //收到ARP reply
      else if ( message.opcode == ARPMessage::OPCODE_REPLY ) {
        //更新ARP表
        ip2ether_.insert( { message.sender_ip_address, { message.sender_ethernet_address, 0 } } );
        auto const& dgram = wait_drgams[message.sender_ip_address]; //提取待发送的数据
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
//更新ARP表和每个目的ip的arp_timer
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  (void)ms_since_last_tick;
  size_t IP_MAP_TTL = 30000;
  size_t ARP_request_TTL = 5000; 

  for ( auto it = ip2ether_.begin(); it != ip2ether_.end(); ) {
    it->second.second += ms_since_last_tick;
    if ( it->second.second >= IP_MAP_TTL ) { //ARP表项超出时间需要删除
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

//网络接口的任务 ：接受来自链路层的 "数据帧"   和来自网络层的  "数据包"
//发送数据包的时候先查找ARP缓存，如果没有找到的话就先查询是否已经发送过ARP请求了，如果还没有发送的话就构造广播帧ARP请求报文
//收到数据帧的时候，先判断MAC地址是不是本接口地址/广播地址，如果是IPV4报 直接返回给调用者 如果是请求，先确保是对自己的请求返回构造回复帧，如果是回复报文，那么先增加到ARP表项中去，然后根据
//目的IP地址直接发送给对方