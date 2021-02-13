#ifndef NET_ETHERNET_H
#define NET_ETHERNET_H

#include <stdint.h>

#define ETHTYPE_ARP		0x0806
#define ETHTYPE_IPv4	0x0800
#define ETHTYPE_IPv6	0x86dd

typedef struct ethernet_frame {
	uint8_t dst_mac_addr[6];
	uint8_t src_mac_addr[6];
	uint16_t type;
	uint8_t data[];
} __attribute__((packed)) ethernet_frame_t;

void ethernet_receive(ethernet_frame_t* ethernet_frame, uint32_t size);

#endif