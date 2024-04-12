#include "router.hh"
#include <sys/types.h>

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

    // RadixNode *node = &_root;
    std::shared_ptr<RadixNode> node = _root;
    // 找到最长匹配的节点，在此节点下添加新的节点
    size_t already_matched_bits = 0;
    while (node->left_child != nullptr || node->right_child != nullptr)
    {
        if (node->left_child != nullptr)
        {
            uint32_t mask = 0xffffffff << (32 - node->left_child->prefix_length);
            if ((route_prefix & mask) == node->left_child->route_prefix)
            {
                already_matched_bits = node->left_child->prefix_length;
                node = node->left_child;
                continue;
            }
        }
        if (node->right_child != nullptr)
        {
            uint32_t mask = 0xffffffff << (32 - node->right_child->prefix_length);
            if ((route_prefix & mask) == node->right_child->route_prefix)
            {
                already_matched_bits = node->right_child->prefix_length;
                node = node->right_child;
                continue;
            }
        }
        break;
    }

    if (already_matched_bits == prefix_length)
    {
        node->next_hop = next_hop;
        node->interface_num = interface_num;
        node->is_real_route = true;
    }
    else
    {
        // 第already_matched_bits位为0，添加左节点；为1，添加右节点
        bool next_bit = (route_prefix >> (31 - already_matched_bits)) & 1;
        if (next_bit == 0)
        {
            // 左节点为空，直接添加新节点
            if (node->left_child == nullptr)
            {
                std::shared_ptr<RadixNode> new_node(new RadixNode{route_prefix, prefix_length, next_hop, interface_num, {}, {}, true});
                node->left_child = new_node;
            }
            // 左节点不为空，说明存在重叠，拆分重叠的节点
            else
            {
                std::shared_ptr<RadixNode> overlap_node = node->left_child;
                uint32_t tmp = overlap_node->route_prefix ^ route_prefix;
                uint8_t new_prefix_length = already_matched_bits;
                // 求出从高往低连续的0的个数，即新节点的前缀长度
                while (((tmp >> (31 - new_prefix_length)) & 1) == 0)
                {
                    new_prefix_length++;
                }
                std::shared_ptr<RadixNode> new_node_1(new RadixNode{route_prefix, prefix_length, next_hop, interface_num, {}, {}, true});
                std::shared_ptr<RadixNode> new_node_2(new RadixNode{overlap_node->route_prefix, overlap_node->prefix_length, overlap_node->next_hop,
                                                                    overlap_node->interface_num, overlap_node->left_child, overlap_node->right_child,
                                                                    overlap_node->is_real_route});
                uint32_t mask = 0xffffffff << (32 - new_prefix_length);
                overlap_node->prefix_length = new_prefix_length;
                overlap_node->route_prefix = overlap_node->route_prefix & mask;
                overlap_node->is_real_route = false;

                // 根据新增节点的new_prefix_length+1位是0还是1，分别添加到左右节点
                if (((route_prefix >> (31 - new_prefix_length)) & 1) == 0)
                {
                    overlap_node->left_child = new_node_1;
                    overlap_node->right_child = new_node_2;
                }
                else
                {
                    overlap_node->left_child = new_node_2;
                    overlap_node->right_child = new_node_1;
                }
            }
        }
        else
        {
            // 右节点为空，直接添加新节点
            if (node->right_child == nullptr)
            {
                std::unique_ptr<RadixNode> new_node(new RadixNode{route_prefix, prefix_length, next_hop, interface_num, {}, {}, true});
                node->right_child = std::move(new_node);
            }
            // 右节点不为空，说明存在重叠，拆分重叠的节点
            else
            {
                std::shared_ptr<RadixNode> overlap_node = node->right_child;
                uint32_t tmp = overlap_node->route_prefix ^ route_prefix;
                uint8_t new_prefix_length = already_matched_bits;
                // 求出从高往低连续的0的个数，即新节点的前缀长度
                while (((tmp >> (31 - new_prefix_length)) & 1) == 0)
                {
                    new_prefix_length++;
                }
                std::shared_ptr<RadixNode> new_node_1(new RadixNode{route_prefix, prefix_length, next_hop, interface_num, {}, {}, true});
                std::shared_ptr<RadixNode> new_node_2(new RadixNode{overlap_node->route_prefix, overlap_node->prefix_length, overlap_node->next_hop,
                                                                    overlap_node->interface_num, overlap_node->left_child, overlap_node->right_child,
                                                                    overlap_node->is_real_route});
                uint32_t mask = 0xffffffff << (32 - new_prefix_length);
                overlap_node->prefix_length = new_prefix_length;
                overlap_node->route_prefix = overlap_node->route_prefix & mask;
                overlap_node->is_real_route = false;

                // 根据新增节点的new_prefix_length+1位是0还是1，分别添加到左右节点
                if (((route_prefix >> (31 - new_prefix_length)) & 1) == 0)
                {
                    overlap_node->left_child = new_node_1;
                    overlap_node->right_child = new_node_2;
                }
                else
                {
                    overlap_node->left_child = new_node_2;
                    overlap_node->right_child = new_node_1;
                }
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

    std::shared_ptr<RadixNode> node = _root;
    while (true)
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
        if (node->left_child == nullptr && node->right_child == nullptr)
        {
            break;
        }
        else
        {
            if (node->left_child)
            {
                uint32_t mask = 0xffffffff << (32 - node->left_child->prefix_length);
                if ((dgram.header().dst & mask) == node->left_child->route_prefix)
                {
                    node = node->left_child;
                    continue;
                }
            }
            if (node->right_child)
            {
                uint32_t mask = 0xffffffff << (32 - node->right_child->prefix_length);
                if ((dgram.header().dst & mask) == node->right_child->route_prefix)
                {
                    node = node->right_child;
                    continue;
                }
            }
            // 两个子节点都不匹配，结束
            break;
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
