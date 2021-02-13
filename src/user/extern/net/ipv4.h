#ifndef NET_IPv4_H
#define NET_IPv4_H

#include <stdint.h>
#include <stdbool.h>

#define IPv4_PROTOCOL_ICMP		0x01
#define IPv4_PROTOCOL_IGMP		0x02
#define IPv4_PROTOCOL_TCP		0x06
#define IPv4_PROTOCOL_UDP		0x11

typedef struct ipv4_packet {
	// Low bits are defined first
	// Byte 0
	uint8_t ihl:4;
	uint8_t version:4;

	// Byte 1
	uint8_t ecn:2;
	uint8_t dscp:6;

	// Bytes 2-3
	uint16_t total_length;

	// Bytes 4-5
	uint16_t identification;

	// Bytes 6-7
	uint16_t fragment_off:13;
	uint16_t flags:3;

	// Byte 8
	uint8_t time_to_live;
	// Byte 9
	uint8_t protocol;

	// Bytes 10-11
	uint16_t header_checksum;

	// Bytes 12-15
	uint32_t source_ip;
	// Bytes 16-19
	uint32_t dest_ip;

	// Note: Options are not modelled here, and we ignore a packet if they're received 

	uint8_t data[];
} __attribute((packed)) ipv4_packet_t;

void ipv4_receive(ipv4_packet_t* packet, uint32_t packet_size);

bool ip_equals__buf_u8(const uint8_t ip_buf[4], uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3);
bool ip_equals__buf_buf(const uint8_t ip_buf[4], uint8_t ip_buf2[4]);
bool ip_equals__buf_u32(const uint8_t ip_buf[4], uint32_t ip2);

#endif