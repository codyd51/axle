#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Assert
#include <libport/libport.h>

#include "ipv4.h"
#include "util.h"
#include "udp.h"
#include "tcp.h"
#include "ethernet.h"
#include "arp.h"

bool ip_equals__buf_u8(const uint8_t ip_buf[IPv4_ADDR_SIZE], uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
	return ip_buf[0] == b0 &&
		   ip_buf[1] == b1 &&
		   ip_buf[2] == b2 &&
		   ip_buf[3] == b3;
}

bool ip_equals__buf_buf(const uint8_t ip_buf[IPv4_ADDR_SIZE], uint8_t ip_buf2[IPv4_ADDR_SIZE]) {
	return !memcmp(ip_buf, ip_buf2, IPv4_ADDR_SIZE);
}

bool ip_equals__buf_u32(const uint8_t ip_buf[IPv4_ADDR_SIZE], uint32_t ip2) {
	return (uint8_t)(ip2 >> 24) == ip_buf[3] && 
		   (uint8_t)(ip2 >> 16) == ip_buf[2] && 
		   (uint8_t)(ip2 >> 8) == ip_buf[1] && 
		   (uint8_t)(ip2 >> 0) == ip_buf[0];
}

void ipv4_receive(packet_info_t* packet_info, ipv4_packet_t* packet, uint32_t packet_size) {
	// Ignore packets intended for other hosts
	uint8_t ipv4[IPv4_ADDR_SIZE];
	net_copy_local_ipv4_addr(ipv4);
	if (memcmp(&packet->dest_ip, ipv4, IPv4_ADDR_SIZE)) {
		free(packet);
		return;
	}

    // Fill in the IPv4 specific packet info
    memcpy(packet_info->src_ipv4, &packet->source_ip, IPv4_ADDR_SIZE);
    memcpy(packet_info->dst_ipv4, &packet->dest_ip, IPv4_ADDR_SIZE);

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
		uint32_t udp_packet_size = ntohs(packet->total_length) - sizeof(ipv4_packet_t);
		udp_packet_t* copied_packet = malloc(udp_packet_size);
		memcpy(copied_packet, packet_body, udp_packet_size);
		udp_receive(packet_info, copied_packet, udp_packet_size);
	}
	else if (packet->protocol == IPv4_PROTOCOL_TCP) {
		tcp_packet_t* packet_body = (tcp_packet_t*)&packet->data;
		uint32_t tcp_packet_size = ntohs(packet->total_length) - sizeof(ipv4_packet_t);
		tcp_packet_t* copied_packet = malloc(tcp_packet_size);
		memcpy(copied_packet, packet_body, tcp_packet_size);
		tcp_receive(packet_info, copied_packet, tcp_packet_size);
	}

	free(packet);
}

void ipv4_send(
	const uint8_t dst_ipv4_addr[IPv4_ADDR_SIZE], 
	ipv4_protocol_t protocol, 
	void* packet, 
	uint32_t packet_size
) {
	uint32_t ipv4_packet_size = sizeof(ipv4_packet_t) + packet_size;
	ipv4_packet_t* wrapper = calloc(1, ipv4_packet_size);
	wrapper->version = 4;
	wrapper->ihl = 5;
	wrapper->total_length = htons(ipv4_packet_size);
	//wrapper->identification = htons(0x2123);
	wrapper->identification = 4;
	wrapper->time_to_live = 64;
	wrapper->protocol = protocol;

	//wrapper->source_ip = 0xffffffff;

	wrapper->source_ip = net_copy_local_ipv4_addr__u32();
	memcpy(&wrapper->dest_ip, dst_ipv4_addr, IPv4_ADDR_SIZE);
	wrapper->header_checksum = net_checksum_ipv4(wrapper, offsetof(ipv4_packet_t, data));

	memcpy(wrapper->data, packet, packet_size);

	// Find the router's MAC
	uint8_t router_ip[IPv4_ADDR_SIZE];
	net_copy_router_ipv4_addr(router_ip);
	//uint8_t router_mac[MAC_ADDR_SIZE] = {0x34, 0x27, 0x92, 0x36, 0x8c, 0x61};
	uint8_t router_mac[MAC_ADDR_SIZE];
	assert(arp_copy_mac(router_ip, router_mac), "ARP failed to map the router's MAC");

	ethernet_send(router_mac, ETHTYPE_IPv4, (uint8_t*)wrapper, ipv4_packet_size);
	free(wrapper);
}