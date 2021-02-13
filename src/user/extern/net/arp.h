#ifndef ARP_H
#define ARP_H

#include <stdint.h>

typedef struct arp_entry {
	bool allocated;
	uint8_t mac_addr[6];
	uint8_t ip_addr[4];
} arp_entry_t;

#define ARP_TABLE_SIZE 256
arp_entry_t* arp_table(void);

typedef struct arp_packet {
	uint16_t hardware_type;
	uint16_t protocol_type;
	uint8_t hware_addr_len;
	uint8_t proto_addr_len;
	uint16_t opcode;
	uint8_t sender_hware_addr[6];
	uint8_t sender_proto_addr[4];
	uint8_t target_hware_addr[6];
	uint8_t target_proto_addr[4];
} __attribute__((packed)) arp_packet_t;

void arp_receive(arp_packet_t* packet);

void format_mac_address(char* out, ssize_t out_size, uint8_t mac_addr[6]);
void format_ip_address(char* out, ssize_t out_size, uint8_t ip_addr[6]);

#endif