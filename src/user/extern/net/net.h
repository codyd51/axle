#ifndef NET_H
#define NET_H

#define MAC_ADDR_SIZE   6
#define IPv4_ADDR_SIZE  4

typedef struct net_config {
    // Local link
    uint8_t nic_mac[MAC_ADDR_SIZE];
    uint8_t ip_addr[IPv4_ADDR_SIZE];

    // Router
    uint8_t router_ip_addr[IPv4_ADDR_SIZE];
} net_config_t;

typedef struct packet_info {
    // Added by ethernet layer
    uint8_t src_mac[MAC_ADDR_SIZE];
    uint8_t dst_mac[MAC_ADDR_SIZE];
    // Added by IPv4 layer
    uint8_t src_ipv4[IPv4_ADDR_SIZE];
    uint8_t dst_ipv4[IPv4_ADDR_SIZE];

    // Added by UDP layer
    uint16_t src_udp_port;
    uint16_t dst_udp_port;
} packet_info_t;

void net_copy_local_mac_addr(uint8_t dest[MAC_ADDR_SIZE]);
void net_copy_local_ipv4_addr(uint8_t dest[IPv4_ADDR_SIZE]);
uint32_t net_copy_local_ipv4_addr__u32(void);
void net_copy_router_ipv4_addr(uint8_t dest[IPv4_ADDR_SIZE]);

#endif