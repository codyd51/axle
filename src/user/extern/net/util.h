#ifndef NET_UTIL_H
#define NET_UTIL_H

#include <stdint.h>
#include <unistd.h>

#include "net.h"

void format_mac_address(char* out, ssize_t out_size, uint8_t mac_addr[6]);

void format_ipv4_address__buf(char* out, ssize_t out_size, uint8_t ip_addr[4]);
void format_ipv4_address__u32(char* out, ssize_t out_size, uint32_t ip_addr);

void hexdump(const void* addr, const int len);

uint16_t net_checksum_ipv4(void* addr, int count);
// TODO(PT): This produces the wrong checksum and needs fixing
uint16_t net_checksum_tcp_udp(uint16_t proto, 
                              uint16_t length, 
                              uint8_t src_ip[IPv4_ADDR_SIZE], 
                              uint8_t dst_ip[IPv4_ADDR_SIZE],
                              void* addr, 
                              int count);

#endif