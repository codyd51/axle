#ifndef NET_ETHERNET_H
#define NET_ETHERNET_H

#include <stdint.h>
#include "net.h"

typedef enum ethtype {
    ETHTYPE_ARP     = 0x0806,
    ETHTYPE_IPv4    = 0x0800,
    ETHTYPE_IPv6    = 0x86dd
} ethtype_t;

typedef struct ethernet_frame {
	uint8_t dst_mac_addr[MAC_ADDR_SIZE];
	uint8_t src_mac_addr[MAC_ADDR_SIZE];
	uint16_t type;
	uint8_t data[];
} __attribute__((packed)) ethernet_frame_t;

void ethernet_receive(packet_info_t* packet_info, ethernet_frame_t* ethernet_frame, uint32_t size);
void ethernet_send(uint8_t dst_mac_addr[MAC_ADDR_SIZE], ethtype_t ethtype, uint8_t* packet, uint32_t size);

#endif