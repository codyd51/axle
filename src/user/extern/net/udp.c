#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Port IO
#include <libport/libport.h>

#include "udp.h"
#include "ipv4.h"
#include "dns.h"
#include "util.h"

#define UDP_DNS_PORT    53
#define UDP_MDNS_PORT   5353

const uint8_t MDNS_DESTINATION_IP[4] = {224, 0, 0, 251};

void udp_receive(packet_info_t* packet_info, udp_packet_t* packet, uint32_t packet_size) {
	printf("UDP packet SrcPort[%d] DstPort[%d] Len[%d]\n", ntohs(packet->source_port), ntohs(packet->dest_port), ntohs(packet->length));

	if (ip_equals__buf_buf(MDNS_DESTINATION_IP, packet_info->dst_ipv4)) {
		//printf("MDNS!!!\n");
	}

	if (ntohs(packet->dest_port) == UDP_DNS_PORT || 
		ntohs(packet->source_port) == UDP_DNS_PORT ||
		ntohs(packet->dest_port) == UDP_MDNS_PORT ||
		ntohs(packet->source_port) == UDP_MDNS_PORT) {
		dns_packet_t* packet_body = (dns_packet_t*)&packet->data;
		uint32_t dns_packet_size = packet_size - offsetof(udp_packet_t, data);
		dns_packet_t* copied_packet = malloc(dns_packet_size);
		memcpy(copied_packet, packet_body, dns_packet_size);
		dns_receive(packet_info, copied_packet, dns_packet_size);
	}

	free(packet);
}

void udp_send(void* packet, 
			  uint32_t packet_size, 
			  uint16_t source_port, 
			  uint16_t dest_port, 
			  uint8_t dst_ip[IPv4_ADDR_SIZE]) {
	uint32_t udp_packet_size = sizeof(udp_packet_t) + packet_size;
	udp_packet_t* wrapper = malloc(udp_packet_size);
	memset(wrapper, 0, udp_packet_size);

	wrapper->source_port = htons(source_port);
	wrapper->dest_port = htons(dest_port);
	wrapper->length = htons(udp_packet_size);
	memcpy(wrapper->data, packet, packet_size);

	/*
	uint8_t src_ip[IPv4_ADDR_SIZE] = {0};
	net_copy_local_ipv4_addr(src_ip);
	wrapper->checksum = htons(net_checksum_tcp_udp2(
		src_ip,
		dst_ip,
		IPv4_PROTOCOL_UDP,
		udp_packet_size,
		source_port,
		dest_port,
		packet, packet_size
	));
	/*
	wrapper->checksum = net_checksum_tcp_udp(IPv4_PROTOCOL_UDP,
											 udp_packet_size,
											 src_ip,
											 dst_ip,
											 wrapper,
											 udp_packet_size);
											 */
	ipv4_send(dst_ip, IPv4_PROTOCOL_UDP, wrapper, udp_packet_size);
	free(wrapper);
}
