#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Port IO
#include <libport/libport.h>

#include "ethernet.h"

// Protocols
#include "arp.h"
#include "ipv4.h"

void ethernet_receive(ethernet_frame_t* ethernet_frame, uint32_t size) {
	uint16_t ethtype = ntohs(ethernet_frame->type);
	const char* ethtype_name = "?";
	switch (ethtype) {
		case ETHTYPE_ARP:
			ethtype_name = "ARP";
			break;
		case ETHTYPE_IPv4:
			ethtype_name = "IPv4";
			break;
		case ETHTYPE_IPv6:
			ethtype_name = "IPv6";
			break;
		default:
			ethtype_name = "?";
			break;
	}
	char dst_mac_buf[64] = {0};
	snprintf(
		dst_mac_buf, 
		sizeof(dst_mac_buf), 
		"%02x:%02x:%02x:%02x:%02x:%02x", 
		ethernet_frame->dst_mac_addr[0],
		ethernet_frame->dst_mac_addr[1],
		ethernet_frame->dst_mac_addr[2],
		ethernet_frame->dst_mac_addr[3],
		ethernet_frame->dst_mac_addr[4],
		ethernet_frame->dst_mac_addr[5]
	);
	char src_mac_buf[64] = {0};
	snprintf(
		src_mac_buf, 
		sizeof(src_mac_buf), 
		"%02x:%02x:%02x:%02x:%02x:%02x", 
		ethernet_frame->src_mac_addr[0],
		ethernet_frame->src_mac_addr[1],
		ethernet_frame->src_mac_addr[2],
		ethernet_frame->src_mac_addr[3],
		ethernet_frame->src_mac_addr[4],
		ethernet_frame->src_mac_addr[5]
	);

	/*
	printf("\tDestination MAC: %s\n", dst_mac_buf);
	printf("\tSource MAC: %s\n", src_mac_buf);
	printf("\tEthType: 0x%04x (%s)\n", ethtype, ethtype_name);
	*/

	if (ethtype == ETHTYPE_ARP) {
		// Strip off the Ethernet header and pass along the packet to ARP
		arp_packet_t* packet_body = (arp_packet_t*)&ethernet_frame->data;
		uint32_t arp_packet_size = size - offsetof(ethernet_frame_t, data);
		arp_packet_t* copied_packet = malloc(arp_packet_size);
		memcpy(copied_packet, packet_body, arp_packet_size);
		arp_receive(copied_packet);
	}
	else if (ethtype == ETHTYPE_IPv4) {
		// Strip off the Ethernet header and pass along the packet to IPv4
		ipv4_packet_t* packet_body = (ipv4_packet_t*)&ethernet_frame->data;
		uint32_t ipv4_packet_size = size - offsetof(ethernet_frame_t, data);
		ipv4_packet_t* copied_packet = malloc(ipv4_packet_size);
		memcpy(copied_packet, packet_body, ipv4_packet_size);
		ipv4_receive(copied_packet, ipv4_packet_size);
	}

	free(ethernet_frame);
}
