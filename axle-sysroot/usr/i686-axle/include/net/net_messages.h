#ifndef NET_MESSAGES_H
#define NET_MESSAGES_H

#include <kernel/amc.h>

#define NET_SERVICE_NAME "com.axle.net"

// Sent from the NIC to the net stack
#define NET_RX_ETHERNET_FRAME (1 << 0)

// Sent from the net stack to the NIC
#define NET_TX_ETHERNET_FRAME (1 << 1)

// Sent from the net stack to the NIC
#define NET_REQUEST_NIC_CONFIG  (1 << 2)

// Sent from the NIC to the net stack
#define NET_RESPONSE_NIC_CONFIG (1 << 3)

typedef struct net_message_common {
    uint8_t event;
} net_message_common_t;

typedef struct net_packet {
    net_message_common_t common;
    uint32_t len;
    uint8_t data[];
} net_packet_t;

typedef struct net_nic_config_info {
    net_message_common_t common;
    uint8_t mac_addr[6];
} net_nic_config_info_t;

typedef union net_message {
    net_packet_t packet;
    net_nic_config_info_t config_info;
} net_message_t;

#endif
