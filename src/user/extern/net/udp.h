#ifndef NET_UDP_H
#define NET_UDP_H

typedef struct udp_packet {
	uint16_t source_port;
	uint16_t dest_port;
	uint16_t length;
	uint16_t checksum;
	uint8_t data[];
} __attribute((packed)) udp_packet_t;

void udp_receive(udp_packet_t* packet, uint32_t packet_size, uint32_t source_ip, uint32_t dest_ip);

#endif