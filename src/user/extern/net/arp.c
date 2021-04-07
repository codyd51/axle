#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Port IO
#include <libport/libport.h>

#include <stdlibadd/array.h>

#include "net.h"
#include "net_messages.h"
#include "arp.h"
#include "ethernet.h"
#include "util.h"
#include "callback.h"

#define ARP_REQUEST	0x1
#define ARP_REPLY 	0x2

#define ARP_HWARE_TYPE_ETHERNET	0x1
#define ARP_PROTO_TYPE_IPv4		0x0800

static arp_entry_t _arp_table[ARP_TABLE_SIZE] = {0};

static void* _arp_callbacks = 0;

bool arp_copy_mac(uint8_t ip_addr[IPv4_ADDR_SIZE], uint8_t out_mac[MAC_ADDR_SIZE]) {
	char b1[64];
	char b2[64];
	format_ipv4_address__buf(b1, 64, ip_addr);
	format_mac_address(b2, 64, out_mac);
	// Search for the provided IP address in the ARP cache
	for (int i = 0; i < ARP_TABLE_SIZE; i++) {
		arp_entry_t* ent = &_arp_table[i];
		if (ent->allocated) {
			if (!memcmp(ip_addr, ent->ip_addr, IPv4_ADDR_SIZE)) {
				// Copy the MAC to the output buffer
				memcpy(out_mac, ent->mac_addr, MAC_ADDR_SIZE);
				return true;
			}
		}
	}
	return false;
}

bool arp_cache_contains_ipv4(uint8_t ip_addr[IPv4_ADDR_SIZE]) {
	char sender_ip[64];
	format_ipv4_address__buf(sender_ip, sizeof(sender_ip), ip_addr);
	for (int i = 0; i < ARP_TABLE_SIZE; i++) {
		arp_entry_t* ent = &_arp_table[i];
		if (ent->allocated) {
			if (!memcmp(ip_addr, ent->ip_addr, IPv4_ADDR_SIZE)) {
				return true;
			}
		}
	}
	return false;
}

static void _update_arp_table(uint8_t mac_addr[6], uint8_t ip_addr[4]) {
	printf("Inserting ARP entry\n");
	// If this IP is already in the table, update the associated MAC
	for (int i = 0; i < ARP_TABLE_SIZE; i++) {
		arp_entry_t* ent = &_arp_table[i];
		if (ent->allocated) {
			if (!memcmp(ip_addr, ent->ip_addr, 4)) {
				// Update the MAC
				memcpy(ent->mac_addr, mac_addr, 6);
				return;
			}
		}
	}

	// Find a free entry in the table
	for (int i = 0; i < ARP_TABLE_SIZE; i++) {
		arp_entry_t* ent = &_arp_table[i];
		if (!ent->allocated) {
			ent->allocated = true;
			memcpy(ent->mac_addr, mac_addr, 6);
			memcpy(ent->ip_addr, ip_addr, 4);
			return;
		}
	}
	assert(false, "Failed to find a free ARP slot\n");
}

arp_entry_t* arp_table(void) {
	return _arp_table;
}

void arp_receive(packet_info_t* packet_info, arp_packet_t* arp_packet) {
	char sender_ip[64];
	format_ipv4_address__buf(sender_ip, sizeof(sender_ip), arp_packet->sender_proto_addr);
	printf("ARP received from %s\n", sender_ip);

	assert(ntohs(arp_packet->hardware_type) == ARP_HWARE_TYPE_ETHERNET, "Unknown ARP hardware type");
	assert(ntohs(arp_packet->protocol_type) == ARP_PROTO_TYPE_IPv4, "Unknown ARP protocol type");
	assert(arp_packet->hware_addr_len == 6, "MAC address was not 6 bytes");
	assert(arp_packet->proto_addr_len == 4, "IPv4 address was not 4 bytes");

	uint16_t opcode = ntohs(arp_packet->opcode);
	if (opcode == ARP_REPLY) {
		// Update our ARP table with the sender's info
		_update_arp_table(arp_packet->sender_hware_addr, 
						  arp_packet->sender_proto_addr);
		_update_arp_table(arp_packet->target_hware_addr, 
						  arp_packet->target_proto_addr);
        // We may have unblocked a callback waiting on this ARP reply
        callback_list_invoke_ready_callbacks(_arp_callbacks);
	}
	else if (opcode == ARP_REQUEST) {
		_update_arp_table(arp_packet->sender_hware_addr, 
						  arp_packet->sender_proto_addr);
	}
	else {
		printf("Unknown ARP opcode %d\n", opcode);
	}
	/*
	printf("** ARP packet! **\n");

	printf("HW type 		0x%04x\n", ntohs(arp_packet->hardware_type));
	printf("Proto type 		0x%04x\n", ntohs(arp_packet->protocol_type));
	printf("HW_addr len		%d\n", arp_packet->hware_addr_len);
	printf("Proto addr len  %d\n", arp_packet->proto_addr_len);
	printf("Opcode		    %d\n", ntohs(arp_packet->opcode));
	*/

	char buf[64] = {0};
	format_ipv4_address__buf(buf, sizeof(buf), arp_packet->sender_proto_addr);
	printf("ARP Sender %s ", buf);
	format_mac_address(buf, sizeof(buf), arp_packet->sender_hware_addr);
	printf("[%s], target ", buf);
	format_ipv4_address__buf(buf, sizeof(buf), arp_packet->target_proto_addr);
	printf("%s ", buf);
	format_mac_address(buf, sizeof(buf), arp_packet->target_hware_addr);
	printf("[%s]\n", buf);

	free(arp_packet);
}

void arp_request_mac(uint8_t dst_ip_addr[IPv4_ADDR_SIZE]) {
	arp_packet_t req = {0};
	req.hardware_type = htons(ARP_HWARE_TYPE_ETHERNET);
	req.protocol_type = htons(ARP_PROTO_TYPE_IPv4);
	req.hware_addr_len = MAC_ADDR_SIZE;
	req.proto_addr_len = IPv4_ADDR_SIZE;
	req.opcode = htons(ARP_REQUEST);
	net_copy_local_mac_addr(req.sender_hware_addr);
	net_copy_local_ipv4_addr(req.sender_proto_addr);
	memcpy(req.target_proto_addr, dst_ip_addr, IPv4_ADDR_SIZE);

	uint8_t global_broadcast_mac[MAC_ADDR_SIZE] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	ethernet_send(global_broadcast_mac, ETHTYPE_ARP, (uint8_t*)&req, sizeof(arp_packet_t));
}

void arp_announce(void) {
	// In an ARP announcement, the target MAC address can be left as zero
	arp_packet_t announcement = {0};
	announcement.hardware_type = htons(ARP_HWARE_TYPE_ETHERNET);
	announcement.protocol_type = htons(ARP_PROTO_TYPE_IPv4);
	announcement.hware_addr_len = MAC_ADDR_SIZE;
	announcement.proto_addr_len = IPv4_ADDR_SIZE;
	announcement.opcode = htons(ARP_REQUEST);
	net_copy_local_mac_addr(announcement.sender_hware_addr);
	net_copy_local_ipv4_addr(announcement.sender_proto_addr);
	net_copy_local_ipv4_addr(announcement.target_proto_addr);

	uint8_t global_broadcast_mac[MAC_ADDR_SIZE] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	ethernet_send(global_broadcast_mac, ETHTYPE_ARP, (uint8_t*)&announcement, sizeof(arp_packet_t));

typedef struct arp_amc_rpc_info_t {
	uint8_t ipv4[IPv4_ADDR_SIZE];
	char* amc_service;
} arp_amc_rpc_info_t;

static bool _arp_is_amc_rpc_satisfied(arp_amc_rpc_info_t* rpc) {
	// Is the request IPv4 address in the ARP cache?
    printf("_arp_is_amc_rpc_satisfied\n");
	return arp_cache_contains_ipv4(rpc->ipv4);
}

static void _arp_complete_amc_rpc(arp_amc_rpc_info_t* rpc) {
	printf("arp_complete_amc_rpc for %s, ipv4 0x%08x 0x%08x\n", rpc->amc_service, rpc->ipv4, *rpc->ipv4);
    printf("ARP RPC: MAC is in the ARP cache, responding to %s...\n", rpc->amc_service);
	uint8_t mac[MAC_ADDR_SIZE];
	assert(arp_copy_mac(rpc->ipv4, mac), "Failed to map IPv4 to MAC");
	net_send_rpc_response(rpc->amc_service, NET_RPC_RESPONSE_ARP_GET_MAC, mac, MAC_ADDR_SIZE);

    // Free the buffers we stored
	free(rpc->amc_service);
	free(rpc);
}

void arp_perform_amc_rpc__discover_mac(const char* source_service, uint8_t (*ipv4)[IPv4_ADDR_SIZE]) {
    // Set up a callback for when we receive an ARP reply
	arp_amc_rpc_info_t* cb_info = malloc(sizeof(arp_amc_rpc_info_t));
	memcpy(cb_info->ipv4, ipv4, IPv4_ADDR_SIZE);
    cb_info->amc_service = strndup(source_service, AMC_MAX_SERVICE_NAME_LEN);

    callback_list_add_callback(
        _arp_callbacks,
        cb_info,
        (cb_is_satisfied_func)_arp_is_amc_rpc_satisfied,
        (cb_complete_func)_arp_complete_amc_rpc
    );

	// Check in on the callback immediately in case the mapping was already in the ARP cache
    if (callback_list_invoke_callback_if_ready(_arp_callbacks, cb_info)) {
        return;
    }

	// Send an ARP request for this IPv4 address
	printf("ARP RPC: IPv4 was unknown, broadcasting ARP request...\n");
	arp_request_mac(*ipv4);
}

void arp_init(void) {
	_arp_callbacks = callback_list_init();
}
