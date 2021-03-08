#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Port IO
#include <libport/libport.h>

#include "net.h"
#include "net_messages.h"
#include "tcp.h"
#include "ethernet.h"
#include "util.h"
#include "ipv4.h"

void tcp_send(void) {
	tcp_header_t header = {0};
	header.source_port = htons(1234);
	header.dest_port = htons(80);

	header.sequence_number = 1;
	header.acknowledgement_number = 0;

	uint16_t header_length = sizeof(tcp_header_t);
	header.header_length = 20;
	header.synchronize = 1;

	uint32_t* h = &(header.acknowledgement_number);
	h[1] = (24<<4);

	header.window = htons(16);

	uint32_t packet_size = 60;
	uint32_t tcp_packet_size = header_length + packet_size;
	uint8_t* packet = malloc(tcp_packet_size);
	printf("copying header\n");
	memcpy(packet, &header, header_length);
	printf("setting body\n");
	memset(packet + header_length, 'A', packet_size);

	printf("sending buffer\n");
	uint8_t dest_ip[IPv4_ADDR_SIZE];
	net_copy_router_ipv4_addr(&dest_ip);
	ipv4_send(dest_ip, IPv4_PROTOCOL_TCP, packet, tcp_packet_size);
	free(packet);
}