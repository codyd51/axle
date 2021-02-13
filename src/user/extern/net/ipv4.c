#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Assert
#include <libport/libport.h>

#include "ipv4.h"
#include "util.h"
#include "udp.h"

bool ip_equals__buf_u8(const uint8_t ip_buf[4], uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
	return ip_buf[0] == b0 &&
		   ip_buf[1] == b1 &&
		   ip_buf[2] == b2 &&
		   ip_buf[3] == b3;
}

bool ip_equals__buf_buf(const uint8_t ip_buf[4], uint8_t ip_buf2[4]) {
	return !memcmp(ip_buf, ip_buf2, 4);
}

bool ip_equals__buf_u32(const uint8_t ip_buf[4], uint32_t ip2) {
	return (uint8_t)(ip2 >> 24) == ip_buf[3] && 
		   (uint8_t)(ip2 >> 16) == ip_buf[2] && 
		   (uint8_t)(ip2 >> 8) == ip_buf[1] && 
		   (uint8_t)(ip2 >> 0) == ip_buf[0];
}

void ipv4_receive(ipv4_packet_t* packet, uint32_t packet_size) {
	// Version is stored in the high bits
	//printf("Packet version %d IHL %d\n", packet->version, packet->ihl);
	assert(packet->version == 4, "IPv4 packet version must be 4");

	// The IHL specifies the number of 32-bit words (with a minimum of 5 words)
	assert(packet->ihl >= 5, "IPv4 IHL must be at least 5");
	uint32_t ip_header_len = packet->ihl * sizeof(uint32_t);
	// Options aren't properly considered for now,
	// so ignore the packet if they're needed
	if (packet->ihl > 5) {
		printf("Skipping IPv4 packet that has options\n");
		free(packet);
		return;
	}

	//printf("Got IPV4 packet len = %d\n", ip_header_len);
	//printf("Len %d id %d frago %d flags %d ttl %d prot %d chksum %04x\n", ntohs(packet->total_length), ntohs(packet->identification), packet->fragment_off, packet->flags, packet->time_to_live, packet->protocol, ntohs(packet->header_checksum));

	const char* protocol_name = "?";
	switch (packet->protocol) {
		case IPv4_PROTOCOL_ICMP:
			protocol_name = "ICMP";
			break;
		case IPv4_PROTOCOL_IGMP:
			protocol_name = "ICMP";
			break;
		case IPv4_PROTOCOL_TCP:
			protocol_name = "TCP";
			break;
		case IPv4_PROTOCOL_UDP:
			protocol_name = "UDP";
			break;
		default:
			break;
	}

	char buf[64] = {0};
	printf("IPv4 (%s) packet from ", protocol_name);
	format_ipv4_address__u32(buf, sizeof(buf), packet->source_ip);
	printf("%s to ", buf);
	format_ipv4_address__u32(buf, sizeof(buf), packet->dest_ip);
	printf("%s\n", buf);

	// Strip off the IPv4 header and pass along the packet to UDP
	if (packet->protocol == IPv4_PROTOCOL_UDP) {
		udp_packet_t* packet_body = (udp_packet_t*)&packet->data;
		uint32_t udp_packet_size = packet_size - offsetof(ipv4_packet_t, data);
		udp_packet_t* copied_packet = malloc(udp_packet_size);
		memcpy(copied_packet, packet_body, udp_packet_size);
		udp_receive(copied_packet, udp_packet_size, packet->source_ip, packet->dest_ip);
	}

	free(packet);
}
