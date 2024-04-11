#include "router.hh"

#include "address.hh"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */)
{
}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the
//! corresponding bits of the datagram's destination address? \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly
//! attached to the router (in which case, the next hop address should be the datagram's final destination). \param[in] interface_num The index of the interface
//! to send the datagram out on.
void Router::add_route(const uint32_t route_prefix, const uint8_t prefix_length, const optional<Address> next_hop, const size_t interface_num)
{
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length) << " => "
         << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    // _routes.push_back(Route{route_prefix, prefix_length, next_hop, interface_num});

    RadixNode *node = &_root;
    // 找到最长匹配的节点，在此节点下添加新的节点
    size_t already_matched_bits = 0;
    while (!node->children.empty())
    {
        bool need_add_new_child = true;
        for (auto &child : node->children)
        {
            uint32_t mask = 0xffffffff << (32 - child->prefix_length);
            if ((route_prefix & mask) == child->route_prefix)
            {
                already_matched_bits = child->prefix_length;
                node = child.get();
                need_add_new_child = false;
                break;
            }
        }
        if (need_add_new_child)
        {
            break;
        }
    }

    if (already_matched_bits == prefix_length)
    {
        if (node == &_root)
        {
            node->route_prefix = 0;
            node->prefix_length = 0;
            node->next_hop = next_hop;
            node->interface_num = interface_num;
        }
        node->is_real_route = true;
    }
    else
    {
        // node.children为空，直接添加新节点
        if (node->children.empty())
        {
            std::unique_ptr<RadixNode> new_node(new RadixNode{route_prefix, prefix_length, next_hop, interface_num});
            new_node->is_real_route = true;
            node->children.push_back(std::move(new_node));
        }
        else
        {
            // 检测node.children中是否有和新节点有重叠的节点，如果有，将重叠的节点拆分
            RadixNode *overlap_node = nullptr;
            int overlap_bits = 0;
            for (auto &child : node->children)
            {
                uint32_t tmp = route_prefix & child->route_prefix;
                // 计算tmp从高位开始连续的1的个数
                int count = 0;
                for (int i = 31; i >= 0; i--)
                {
                    if ((tmp & (1 << i)) != 0)
                    {
                        count++;
                    }
                    else
                    {
                        break;
                    }
                }
                if (count > node->prefix_length)
                {
                    overlap_node = child.get();
                    overlap_bits = count;
                }
            }
            // 拆分重叠的节点
            if (overlap_node != nullptr)
            {
                std::unique_ptr<RadixNode> new_node_1(new RadixNode{overlap_node->route_prefix,
                                                                    overlap_node->prefix_length,
                                                                    overlap_node->next_hop,
                                                                    overlap_node->interface_num,
                                                                    {},
                                                                    overlap_node->is_real_route});
                new_node_1->children = std::move(overlap_node->children);
                std::unique_ptr<RadixNode> new_node_2(new RadixNode{route_prefix, prefix_length, next_hop, interface_num});
                new_node_2->is_real_route = true;

                overlap_node->route_prefix = overlap_node->route_prefix & (0xffffffff << (32 - overlap_bits));
                overlap_node->prefix_length = overlap_bits;
                overlap_node->is_real_route = false;
                overlap_node->children.clear();
                overlap_node->children.push_back(std::move(new_node_1));
                overlap_node->children.push_back(std::move(new_node_2));
            }
            // 没有重叠的节点，直接添加新节点
            else
            {
                std::unique_ptr<RadixNode> new_node(new RadixNode{route_prefix, prefix_length, next_hop, interface_num});
                new_node->is_real_route = true;
                node->children.push_back(std::move(new_node));
            }
        }
    }
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram)
{
    if (dgram.header().ttl == 0 || dgram.header().ttl == 1)
    {
        return;
    }
    dgram.header().ttl -= 1;

    optional<Address> next_hop;
    size_t max_match_interface = 0;
    // size_t max_match_bits = 0;
    // for (auto &route : _routes)
    // {
    //     if (route.prefix_length < max_match_bits)
    //     {
    //         continue;
    //     }

    //     uint32_t mask = 0xffffffff << (32 - route.prefix_length);
    //     if ((dgram.header().dst & mask) == route.route_prefix || route.prefix_length == 0)
    //     {
    //         if (route.prefix_length >= max_match_bits)
    //         {
    //             max_match_bits = route.prefix_length;
    //             max_match_interface = route.interface_num;
    //             if (route.next_hop.has_value())
    //             {
    //                 next_hop = route.next_hop;
    //             }
    //             else
    //             {
    //                 next_hop = Address::from_ipv4_numeric(dgram.header().dst);
    //             }
    //         }
    //     }
    // }

    RadixNode *node = &_root;
    bool need_continue = true;
    while (need_continue)
    {
        if (node->is_real_route)
        {
            max_match_interface = node->interface_num;
            if (node->next_hop.has_value())
            {
                next_hop = node->next_hop;
            }
            else
            {
                next_hop = Address::from_ipv4_numeric(dgram.header().dst);
            }
        }
        if (node->children.empty())
        {
            need_continue = false;
            break;
        }
        else
        {
            need_continue = false;
            for (auto &child : node->children)
            {
                uint32_t mask = 0xffffffff << (32 - child->prefix_length);
                if ((dgram.header().dst & mask) == child->route_prefix || child->prefix_length == 0)
                {
                    node = child.get();
                    need_continue = true;
                    break;
                }
            }
            if (!need_continue)
            {
                if (node->is_real_route)
                {
                    max_match_interface = node->interface_num;
                    if (node->next_hop.has_value())
                    {
                        next_hop = node->next_hop;
                    }
                    else
                    {
                        next_hop = Address::from_ipv4_numeric(dgram.header().dst);
                    }
                }
            }
        }
    }

    if (next_hop.has_value())
    {
        _interfaces.at(max_match_interface).send_datagram(dgram, next_hop.value());
    }
}

void Router::route()
{
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces)
    {
        auto &queue = interface.datagrams_out();
        while (not queue.empty())
        {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
