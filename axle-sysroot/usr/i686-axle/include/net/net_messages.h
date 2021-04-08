#ifndef NET_MESSAGES_H
#define NET_MESSAGES_H

#include <kernel/amc.h>

#define NET_SERVICE_NAME "com.axle.net"

// Sent from the NIC to the net stack
#define NET_RX_ETHERNET_FRAME 1
// Sent from the net stack to the NIC
#define NET_TX_ETHERNET_FRAME 2
typedef struct net_packet {
    uint32_t len;
    uint8_t data[];
} net_packet_t;

// Sent from the net stack to the NIC
#define NET_REQUEST_NIC_CONFIG  3
// Sent from the NIC to the net stack
#define NET_RESPONSE_NIC_CONFIG 4
typedef struct net_nic_config_info {
    uint8_t mac_addr[6];
} net_nic_config_info_t;


// Sent from libnet to the net backend
#define NET_RPC_COPY_LOCAL_MAC      5
#define NET_RPC_COPY_LOCAL_IPv4     6
#define NET_RPC_COPY_ROUTER_IPv4    7

#define NET_RPC_ARP_GET_MAC     8
#define NET_RPC_DNS_GET_IPv4    9
#define NET_RPC_TCP_OPEN        10
#define NET_RPC_TCP_SEND        11
#define NET_RPC_TCP_READ        12

// Sent from the net backend to libnet
#define NET_RPC_RESPONSE_COPY_LOCAL_MAC     5
#define NET_RPC_RESPONSE_COPY_LOCAL_IPv4    6
#define NET_RPC_RESPONSE_COPY_ROUTER_IPv4   7

#define NET_RPC_RESPONSE_ARP_GET_MAC        8
#define NET_RPC_RESPONSE_DNS_GET_IPv4       9
#define NET_RPC_RESPONSE_TCP_OPEN           10
#define NET_RPC_RESPONSE_TCP_SEND           11
#define NET_RPC_RESPONSE_TCP_READ           12

typedef struct net_rpc {
    uint32_t len;
    uint8_t data[];
} net_rpc_t;

typedef struct net_rpc_tcp_open {
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t dst_ipv4[4];
} net_rpc_tcp_open_t;

typedef struct net_rpc_tcp_open_response {
    uint32_t tcp_conn_descriptor;
} net_rpc_tcp_open_response_t;

typedef struct net_rpc_tcp_send {
    uint32_t tcp_conn_descriptor;
    uint32_t len;
    uint8_t data[];
} net_rpc_tcp_send_t;

typedef struct net_rpc_tcp_read {
    uint32_t tcp_conn_descriptor;
    uint32_t len;
} net_rpc_tcp_read_t;

typedef struct net_rpc_tcp_read_response {
    uint32_t tcp_conn_descriptor;
    uint32_t len;
    uint8_t data[];
} net_rpc_tcp_read_response_t;

typedef union net_message_body {
    net_packet_t packet;
    net_nic_config_info_t config_info;
    net_rpc_t rpc;
    net_rpc_tcp_open_t tcp_open;
    net_rpc_tcp_open_response_t tcp_open_response;
    net_rpc_tcp_send_t tcp_send;
    net_rpc_tcp_read_t tcp_read;
    net_rpc_tcp_read_response_t tcp_read_response;
} net_message_body_t;

typedef struct net_message {
    uint32_t event;
    net_message_body_t m;
} net_message_t;

#endif
