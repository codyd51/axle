#ifndef LIBNET_H
#define LIBNET_H

#include <stdint.h>

#define MAC_ADDR_SIZE   6
#define IPv4_ADDR_SIZE  4

// Local link / router information
void net_copy_local_mac_addr(uint8_t dest[MAC_ADDR_SIZE]);
void net_copy_local_ipv4_addr(uint8_t dest[IPv4_ADDR_SIZE]);
void net_copy_router_ipv4_addr(uint8_t dest[IPv4_ADDR_SIZE]);

// ARP resolution
void net_get_mac_from_ipv4(uint8_t ipv4_addr[IPv4_ADDR_SIZE], uint8_t out_mac_addr[MAC_ADDR_SIZE]);

// DNS resolution
void net_get_ipv4_of_domain_name(const char* domain_name, uint32_t domain_name_len, uint8_t out_ipv4[IPv4_ADDR_SIZE]);

// TCP
typedef enum {
    TCP_SYN_SENT = 0,
    TCP_SYN_RECV = 1,
} tcp_conn_state_t;

uint16_t net_find_free_port();
uint32_t net_tcp_conn_init(uint16_t src_port, uint16_t dst_port, uint8_t dest_ipv4[IPv4_ADDR_SIZE]);
void net_tcp_conn_send(uint32_t conn_descriptor, uint8_t* data, uint32_t len);
uint32_t net_tcp_conn_read(uint32_t conn_descriptor, uint8_t* buf, uint32_t len);

// Utilities
void format_mac_address(char* out, ssize_t out_size, uint8_t mac_addr[MAC_ADDR_SIZE]);
void format_ipv4_address__buf(char* out, ssize_t out_size, uint8_t ip_addr[IPv4_ADDR_SIZE]);

#endif