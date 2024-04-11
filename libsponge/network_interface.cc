#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected
//! to the same network as the destination) (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric()
//! method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    auto it = _arp_table.find(next_hop_ip);
    // 在 ARP 表中找到目标 IP 地址的 MAC 地址
    if (it != _arp_table.end()) {
        EthernetFrame frame;
        frame.header().dst = it->second.mac;
        frame.header().src = _ethernet_address;
        frame.header().type = EthernetHeader::TYPE_IPv4;
        frame.payload() = dgram.serialize();

        _frames_out.push(frame);
    }
    // 如果 ARP 表中没有目标 IP 地址的 MAC 地址，则发送 ARP 请求报文
    else {
        // 已经发送的 ARP 请求报文中没有目标 IP 地址
        if (_arp_requests_without_reply.find(next_hop_ip) == _arp_requests_without_reply.end()) {
            ARPMessage arp_request;
            arp_request.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request.sender_ethernet_address = _ethernet_address;
            arp_request.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request.target_ethernet_address = {};
            arp_request.target_ip_address = next_hop_ip;

            EthernetFrame arp_request_frame;
            arp_request_frame.header().dst = ETHERNET_BROADCAST;
            arp_request_frame.header().src = _ethernet_address;
            arp_request_frame.header().type = EthernetHeader::TYPE_ARP;
            arp_request_frame.payload() = arp_request.serialize();

            _frames_out.push(arp_request_frame);

            _arp_requests_without_reply[next_hop_ip] = _arp_request_ttl;
        }
        // 加入等待队列，收到 ARP 回复后再发送
        _datagrams_waiting_arp.push_back({dgram, next_hop});
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst == _ethernet_address || frame.header().dst == ETHERNET_BROADCAST) {
        auto type = frame.header().type;

        if (type == EthernetHeader::TYPE_IPv4) {
            InternetDatagram datagram;
            datagram.parse(frame.payload());

            return datagram;
        }

        else if (type == EthernetHeader::TYPE_ARP) {
            ARPMessage arp_message;
            arp_message.parse(frame.payload());
            const uint32_t sender_address = arp_message.sender_ip_address;
            const uint32_t target_address = arp_message.target_ip_address;
            const EthernetAddress sender_ethernet_address = arp_message.sender_ethernet_address;
            const EthernetAddress target_ethernet_address = arp_message.target_ethernet_address;

            // ARP 请求报文
            if (arp_message.opcode == ARPMessage::OPCODE_REQUEST) {
                // 发送给自己的 ARP 请求报文
                if (target_address == _ip_address.ipv4_numeric()) {
                    ARPMessage arp_reply;
                    arp_reply.opcode = ARPMessage::OPCODE_REPLY;
                    arp_reply.sender_ethernet_address = _ethernet_address;
                    arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
                    arp_reply.target_ethernet_address = sender_ethernet_address;
                    arp_reply.target_ip_address = sender_address;

                    EthernetFrame arp_reply_frame;
                    arp_reply_frame.header().dst = sender_ethernet_address;
                    arp_reply_frame.header().src = _ethernet_address;
                    arp_reply_frame.header().type = EthernetHeader::TYPE_ARP;
                    arp_reply_frame.payload() = arp_reply.serialize();

                    _frames_out.push(arp_reply_frame);
                    // 更新 ARP 表
                    _arp_table[sender_address] = {sender_ethernet_address, _arp_ttl};
                    _arp_requests_without_reply.erase(sender_address);
                }
            }
            // ARP 回复报文
            else if (arp_message.opcode == ARPMessage::OPCODE_REPLY) {
                // 发送给自己的 ARP 回复报文
                if (target_ethernet_address == _ethernet_address) {
                    // 更新 ARP 表
                    _arp_table[sender_address] = {sender_ethernet_address, _arp_ttl};
                    _arp_requests_without_reply.erase(sender_address);
                    // 根据刚刚收到的 ARP 回复报文，发送等待 ARP 回复的数据报
                    for (auto it = _datagrams_waiting_arp.begin(); it != _datagrams_waiting_arp.end();) {
                        if (it->second.ipv4_numeric() == sender_address) {
                            send_datagram(it->first, it->second);
                            it = _datagrams_waiting_arp.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }
        }
    }

    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    for (auto it = _arp_table.begin(); it != _arp_table.end();) {
        if (it->second.ttl <= ms_since_last_tick) {
            it = _arp_table.erase(it);
        } else {
            it->second.ttl -= ms_since_last_tick;
            ++it;
        }
    }

    for (auto it = _arp_requests_without_reply.begin(); it != _arp_requests_without_reply.end();) {
        if (it->second <= ms_since_last_tick) {
            // 重传 ARP 请求报文
            ARPMessage arp_request;
            arp_request.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request.sender_ethernet_address = _ethernet_address;
            arp_request.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request.target_ethernet_address = {};
            arp_request.target_ip_address = it->first;

            EthernetFrame arp_request_frame;
            arp_request_frame.header().dst = ETHERNET_BROADCAST;
            arp_request_frame.header().src = _ethernet_address;
            arp_request_frame.header().type = EthernetHeader::TYPE_ARP;
            arp_request_frame.payload() = arp_request.serialize();

            _frames_out.push(arp_request_frame);

            _arp_requests_without_reply[it->first] = _arp_request_ttl;
            ++it;
        } else {
            it->second -= ms_since_last_tick;
            ++it;
        }
    }
}
