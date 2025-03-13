#pragma once

#include "network_interface.hh"

#include <optional>
#include <queue>

// A wrapper for NetworkInterface that makes the host-side
// interface asynchronous: instead of returning received datagrams
// immediately (from the `recv_frame` method), it stores them for
// later retrieval. Otherwise, behaves identically to the underlying
// implementation of NetworkInterface.
class AsyncNetworkInterface : public NetworkInterface
{
  std::queue<InternetDatagram> datagrams_in_ {}; //存储接收到的数据报

public:
  using NetworkInterface::NetworkInterface;

  // Construct from a NetworkInterface
  explicit AsyncNetworkInterface( NetworkInterface&& interface ) : NetworkInterface( interface ) {}

  // \brief Receives and Ethernet frame and responds appropriately.

  // - If type is IPv4, pushes to the `datagrams_out` queue for later retrieval by the owner.
  // - If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
  // - If type is ARP reply, learn a mapping from the "target" fields.
  //
  // \param[in] frame the incoming Ethernet frame
  void recv_frame( const EthernetFrame& frame )
  {
    auto optional_dgram = NetworkInterface::recv_frame( frame );
    if ( optional_dgram.has_value() ) {
      datagrams_in_.push( std::move( optional_dgram.value() ) );
    }
  };

  // Access queue of Internet datagrams that have been received
  std::optional<InternetDatagram> maybe_receive()
  {
    if ( datagrams_in_.empty() ) {
      return {};
    }

    InternetDatagram datagram = std::move( datagrams_in_.front() );
    datagrams_in_.pop();
    return datagram;
  }
};

// A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.

class Router   //路由器就是转发+匹配的操作
{
  struct Item
  {   //路由器的表项
    uint32_t route_prefix {}; //路由前缀
    uint8_t prefix_length {}; //前缀长度
    std::optional<Address> next_hop; //下一跳路由地址
    size_t interface_num {}; //网络接口的索引标签
  };
  // The router's collection of network interfaces
  std::vector<AsyncNetworkInterface> interfaces_ {}; //存储路由器的网络接口

public:
  std::vector<Router::Item> routing_table_ {}; //路由表 vector模拟 
  std::vector<Router::Item>::iterator longest_prefix_match_( uint32_t dis_ip ); //根据目标 IP 地址，在路由表中查找最长前缀匹配的表项。
  static int match_length_( uint32_t src_ip, uint32_t dis_ip, uint8_t len ); //计算两个 IP 地址的前缀匹配长度。
  // Add an interface to the router
  // interface: an already-constructed network interface
  // returns the index of the interface after it has been added to the router
  size_t add_interface( AsyncNetworkInterface&& interface ) //添加一个网络接口到路由器 返回添加后的接口索引。
  {
    interfaces_.push_back( std::move( interface ) );
    return interfaces_.size() - 1;
  }

  // Access an interface by index
  AsyncNetworkInterface& interface( size_t N ) { return interfaces_.at( N ); } //直接访问索引为n的接口

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces. For each interface, use the
  // maybe_receive() method to consume every incoming datagram and
  // send it on one of interfaces to the correct next hop. The router
  // chooses the outbound interface and next-hop as specified by the
  // route with the longest prefix_length that matches the datagram's
  // destination address.
  void route();
};
