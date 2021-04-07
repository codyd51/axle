#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Port IO
#include <libport/libport.h>

#include "ethernet.h"

// Protocols
#include "arp.h"
#include "ipv4.h"
#include "net_messages.h"

// Communication with NIC driver
#include <drivers/realtek_8139/rtl8139_messages.h>

void ethernet_receive(packet_info_t* packet_info, ethernet_frame_t* ethernet_frame, uint32_t size) {
    // Fill in the Ethernet data of the packet info with what we see
    memcpy(packet_info->src_mac, ethernet_frame->src_mac_addr, MAC_ADDR_SIZE);
    memcpy(packet_info->dst_mac, ethernet_frame->dst_mac_addr, MAC_ADDR_SIZE);

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

	if (ethtype == ETHTYPE_ARP) {
		// Strip off the Ethernet header and pass along the packet to ARP
		arp_packet_t* packet_body = (arp_packet_t*)&ethernet_frame->data;
		uint32_t arp_packet_size = size - offsetof(ethernet_frame_t, data);
		arp_packet_t* copied_packet = malloc(arp_packet_size);
		memcpy(copied_packet, packet_body, arp_packet_size);
		arp_receive(packet_info, copied_packet);
	}
	else if (ethtype == ETHTYPE_IPv4) {
		// Strip off the Ethernet header and pass along the packet to IPv4
		ipv4_packet_t* packet_body = (ipv4_packet_t*)&ethernet_frame->data;
		uint32_t ipv4_packet_size = size - offsetof(ethernet_frame_t, data);
		ipv4_packet_t* copied_packet = malloc(ipv4_packet_size);
		memcpy(copied_packet, packet_body, ipv4_packet_size);
		ipv4_receive(packet_info, copied_packet, ipv4_packet_size);
	}

	free(ethernet_frame);
}

void ethernet_send(uint8_t dst_mac_addr[MAC_ADDR_SIZE], ethtype_t ethtype, uint8_t* packet, uint32_t packet_size) {
    // Wrap the provided in an Ethernet header
    assert(sizeof(ethernet_frame_t) == 14, "size not as expected");
    uint32_t ethernet_frame_size = sizeof(ethernet_frame_t) + packet_size;
    ethernet_frame_t* wrapper = malloc(ethernet_frame_size);

    memcpy(wrapper->dst_mac_addr, dst_mac_addr, MAC_ADDR_SIZE);

    // Write the NIC's MAC address as the source
    net_copy_local_mac_addr(wrapper->src_mac_addr);

    // Copy in the data the above layer wants to send
    wrapper->type = htons(ethtype);
    memcpy(wrapper->data, packet, packet_size);

	uint32_t total_msg_size = sizeof(net_message_t) + ethernet_frame_size;
	net_message_t* msg = malloc(total_msg_size);
	msg->event = NET_TX_ETHERNET_FRAME;
	msg->m.packet.len = ethernet_frame_size;
	memcpy(&msg->m.packet.data, wrapper, ethernet_frame_size);
	amc_message_construct_and_send(RTL8139_SERVICE_NAME, msg, total_msg_size);
	free(msg);

    free(wrapper);
}
