#ifndef NET_UTIL_H
#define NET_UTIL_H

#include <stdint.h>
#include <unistd.h>

void format_mac_address(char* out, ssize_t out_size, uint8_t mac_addr[6]);

void format_ipv4_address__buf(char* out, ssize_t out_size, uint8_t ip_addr[4]);
void format_ipv4_address__u32(char* out, ssize_t out_size, uint32_t ip_addr);

void hexdump(const void* addr, const int len);

#endif