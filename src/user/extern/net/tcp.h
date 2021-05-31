#ifndef NET_TCP_H
#define NET_TCP_H

#include <stdint.h>
#include "net.h"
#include "net_messages.h"

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PUSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

typedef struct tcp_header {
	uint16_t source_port;
	uint16_t dest_port;

	uint32_t sequence_number;
	uint32_t acknowledgement_number;

	uint8_t unused:4;
	uint8_t header_length:4;

	/*
	uint8_t reserved:2;
	uint8_t urgent:1;
	uint8_t ack:1;
	uint8_t push:1;
	uint8_t reset:1;
	uint8_t synchronize:1;
	uint8_t finish:1;
	*/
	uint8_t flags;

	uint16_t window;
	uint16_t checksum;
	uint16_t urgent_pointer;

	uint16_t options[];
} __attribute__((packed)) tcp_header_t;

typedef struct tcp_packet {
	tcp_header_t header;
	uint8_t data[];
} __attribute((packed)) tcp_packet_t;

void tcp_init(void);

// void tcp_conn_init(uint16_t src_port, uint16_t dst_port, uint8_t dest_ip[IPv4_ADDR_SIZE]);

void tcp_receive(packet_info_t* packet_info, tcp_packet_t* packet, uint32_t packet_size);

void tcp_perform_amc_rpc__conn_open(const char* amc_service, net_rpc_tcp_open_t* tcp_open_msg);
void tcp_perform_amc_rpc__conn_send(const char* amc_service, net_rpc_tcp_send_t* tcp_send_msg);
void tcp_perform_amc_rpc__conn_read(const char* amc_service, net_rpc_tcp_read_t* tcp_read_msg);

#endif