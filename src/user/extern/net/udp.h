#ifndef NET_UDP_H
#define NET_UDP_H

#include "net.h"

typedef struct udp_packet {
	uint16_t source_port;
	uint16_t dest_port;
	uint16_t length;
	uint16_t checksum;
	uint8_t data[];
} __attribute((packed)) udp_packet_t;

void udp_receive(packet_info_t* packet_info, udp_packet_t* packet, uint32_t packet_size);
void udp_send(void* packet, 
			  uint32_t packet_size, 
			  uint16_t source_port, 
			  uint16_t dest_port, 
			  uint8_t dst_ip[IPv4_ADDR_SIZE]);

#endif