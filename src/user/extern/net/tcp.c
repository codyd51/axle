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
#include "tcp.h"
#include "ethernet.h"
#include "util.h"
#include "ipv4.h"
#include "callback.h"

uint32_t inet_addr(uint8_t ip_addr[IPv4_ADDR_SIZE]) {
	uint32_t v=  ip_addr[3] << 24 |
		   ip_addr[2] << 16 |
		   ip_addr[1] << 8 |
		   ip_addr[0] << 0;
	return v;
}

uint32_t sum_every_16bits(uint16_t *addr, int count) {
    register uint32_t sum = 0;
    uint16_t * ptr = addr;
    
    while(count > 1)  {
        sum += *ptr++;
        count -= sizeof(uint16_t);
    }

    /*  Add left-over byte, if any */
    if( count > 0 ) {
		printf("*** TCP Left over byte\n");
        //sum += * (uint8_t *) ptr;
		//sum += *(uint8_t*)addr;
		//sum += ((*(uint8_t*)ptr) & htons(0xFF00));
		//sum += ((*(uint8_t*)ptr)) << sizeof(uint8_t);
		uint8_t* p = (uint8_t*)ptr;
		sum += *p;
	}

    return sum;
}

uint16_t checksum(void *addr, uint32_t count, uint32_t start_sum) {
    /* Compute Internet Checksum for "count" bytes
     *         beginning at location "addr".
     * Taken from https://tools.ietf.org/html/rfc1071
     */
    uint32_t sum = start_sum;

    sum += sum_every_16bits(addr, count);
	printf("\tTCP accumulator: 0x%08x\n", sum);
    
    /*  Fold 32-bit sum to 16 bits */
    while (sum>>16) {
		printf("\tFold 0x%08x -> ", sum);
        sum = ((uint16_t)(sum & 0xffff)) + ((uint16_t)(sum >> 16));
		printf("0x%08x\n", sum);
	}
	printf("\tAfter fold: 0x%08x 0x%08x\n", sum, (uint16_t)~sum);

    return (uint16_t)~sum;
}

typedef struct tcp_pseudo_header {
	uint32_t src_ip;
	uint32_t dst_ip;
	uint8_t reserved;
	uint8_t proto;
	uint16_t packet_len;
} __attribute__((packed)) tcp_pseudo_header_t;

uint16_t tcp_udp_checksum(uint32_t saddr, uint32_t daddr, uint8_t proto,
                     uint8_t *data, uint16_t len) {
    uint32_t sum = 0;

    sum += saddr;
    sum += daddr;
	uint16_t proto16 = proto << sizeof(uint8_t);
    sum += htons(proto16);
    sum += htons(len);
    
    return checksum(data, len, sum);
}

uint32_t tcp_v4_checksum(uint8_t* data, uint16_t len, uint32_t saddr, uint32_t daddr) {
	printf("tcp_v4_checksum data 0x%08x len %d src 0x%08x dst 0x%08x\n", data, len, saddr, daddr);
    return tcp_udp_checksum(saddr, daddr, IPv4_PROTOCOL_TCP, data, len);
}

static array_t* _tcp_conns = 0;
static array_t* _tcp_callbacks = 0;

void tcp_init(void) {
	_tcp_conns = array_create(64);
	_tcp_callbacks = array_create(64);
	srand(ms_since_boot());
}

void tcp_send(
	uint16_t src_port, 
	uint16_t dst_port, 
	uint8_t dest_ip[IPv4_ADDR_SIZE], 
	uint32_t sequence_num,
	uint32_t acknowledge_num,
	uint8_t flags,
	uint8_t* data,
	uint32_t data_len) {
	tcp_header_t header = {0};
	header.source_port = htons(src_port);
	header.dest_port = htons(dst_port);

	uint16_t header_length = sizeof(tcp_header_t);
	header.header_length = header_length / sizeof(uint32_t);
	header.flags = flags;

	header.sequence_number = htonl(sequence_num);
	header.acknowledgement_number = htonl(acknowledge_num);

	//header.sequence_number = 0;
	//header.acknowledgement_number = 0;

	header.window = htons(0xffff);
	//header.window = 0;
	// Set checksum to zero while calculating it
	// https://lateblt.tripod.com/bit34.txt
	header.checksum = 0;

	//uint32_t packet_size = 60;
	uint32_t tcp_packet_size = header_length + data_len;
	//uint32_t tcp_packet_size = header_length;
	uint8_t* packet = calloc(1, tcp_packet_size);
	memcpy(packet, &header, header_length);
	printf("Packet size %d header size %d\n", tcp_packet_size, header_length);

	if (data && data_len) {
		printf("Copying packet data to offset %d\n", header_length);
		memcpy(packet + header_length, data, data_len);
	}

	uint8_t src_ip[IPv4_ADDR_SIZE];
	net_copy_local_ipv4_addr(src_ip);
	//uint16_t chk3 = tcp_v4_checksum(packet, tcp_packet_size, inet_addr(src_ip), inet_addr(dest_ip));

	uint8_t* checksum_buf = calloc(1, tcp_packet_size + sizeof(tcp_pseudo_header_t));
	tcp_pseudo_header_t* pseudo_header = (tcp_pseudo_header_t*)checksum_buf;
	pseudo_header->src_ip = inet_addr(src_ip);
	pseudo_header->dst_ip = inet_addr(dest_ip);
	pseudo_header->proto = 6;
	pseudo_header->packet_len = htons((uint16_t)tcp_packet_size);
	memcpy(checksum_buf + sizeof(tcp_pseudo_header_t), packet, tcp_packet_size);
	uint16_t chk3 = checksum(checksum_buf, tcp_packet_size + sizeof(tcp_pseudo_header_t), 0);
	hexdump(checksum_buf, tcp_packet_size + sizeof(tcp_pseudo_header_t));
	free(checksum_buf);

	printf("chk3 0x%04x\n", chk3);
	header.checksum = chk3;

	// Rewrite the packet to include the checksum
	memcpy(packet, &header, header_length);

	ipv4_send(dest_ip, IPv4_PROTOCOL_TCP, packet, tcp_packet_size);
	free(packet);
}

typedef enum tcp_conn_status {
	TCP_SYN_SENT = 0,
	TCP_SYN_ACK_RECV = 1,
	TCP_ESTABLISHED = 2,
} tcp_conn_status_t;

typedef struct tcp_segment {
	char* buf;
	uint32_t len;
	uint32_t next_to_consume_off;
} tcp_segment_t;

typedef struct tcp_conn {
	uint8_t dest_ip[IPv4_ADDR_SIZE];
	uint16_t src_port;
	uint16_t dst_port;
	uint32_t status;

	uint32_t send_initial_seqnum;
	uint32_t send_oldest_unacked_seqnum;
	uint32_t send_next_seqnum;

	uint32_t recv_initial_seqnum;
	uint32_t recv_next_seqnum;

	array_t* receive_buffer;
} tcp_conn_t;

static tcp_conn_t* _tcp_conn_alloc(uint16_t src_port, uint16_t dst_port, uint8_t dest_ip[IPv4_ADDR_SIZE]) {
	printf("TCP conn_alloc src [%d] dst [%d]\n", src_port, dst_port);
	tcp_conn_t* conn = calloc(1, sizeof(tcp_conn_t));
	memcpy(conn->dest_ip, dest_ip, IPv4_ADDR_SIZE);
	conn->src_port = src_port;
	conn->dst_port = dst_port;
	// Choose an initial sequence number
	conn->send_initial_seqnum = rand();
	conn->send_next_seqnum = conn->send_initial_seqnum;
	conn->receive_buffer = array_create(64);
	array_insert(_tcp_conns, conn);
	return conn;
}

void tcp_conn_init(tcp_conn_t* conn) {
	printf("TCP: tcp_conn_init sending SYN...\n");
	tcp_send(
		conn->src_port,
		conn->dst_port,
		conn->dest_ip,
		conn->send_initial_seqnum,
		conn->recv_initial_seqnum,
		TCP_FLAG_SYN,
		NULL,
		0
	);
}

static void _tcp_complete_handshake(tcp_conn_t* conn, tcp_packet_t* packet) {
	conn->send_next_seqnum += 1;
	uint32_t recv_ack_num = ntohl(packet->header.acknowledgement_number);
	uint32_t recv_seq_num = ntohl(packet->header.sequence_number);
	if (recv_ack_num == conn->send_next_seqnum) {
		printf("\tHandshake step 2: ACK# correct\n");
	}
	else {
		printf("\tHandshake step 2: ACK# incorrect (%d, expected %d)\n", recv_ack_num, conn->send_next_seqnum);
		// TODO(PT): retransmit?
		return;
	}
	conn->recv_initial_seqnum = recv_seq_num;
	conn->recv_next_seqnum = recv_seq_num + 1;
	conn->status = TCP_ESTABLISHED;
	tcp_send(
		conn->src_port,
		conn->dst_port,
		conn->dest_ip,
		conn->send_next_seqnum,
		conn->recv_next_seqnum,
		TCP_FLAG_ACK,
		NULL,
		0
	);
	
	// The connection has been established - kick off any callbacks that 
	// were awaiting this connection setup
	callback_list_invoke_ready_callbacks(_tcp_callbacks);
}

static void _tcp_handle_syn_response(tcp_conn_t* conn, tcp_packet_t* packet) {
	if (packet->header.flags & TCP_FLAG_SYN && packet->header.flags && TCP_FLAG_ACK) {
		printf("\tSynAck SeqNum [%u] AckNum [%u]\n", ntohl(packet->header.sequence_number), ntohl(packet->header.acknowledgement_number));
		printf("\tReceived SynAck for conn, will send Ack\n");
		_tcp_complete_handshake(conn, packet);
	}
	else {
		printf("\tUnexpected response to SYN, sending RST...\n");
		uint32_t recv_ack_num = ntohl(packet->header.acknowledgement_number);
		uint32_t recv_seq_num = ntohl(packet->header.sequence_number);
		tcp_send(
			conn->src_port,
			conn->dst_port,
			conn->dest_ip,
			recv_ack_num,
			0,
			TCP_FLAG_RST,
			NULL,
			0
		);
		// Restart the handshake
		tcp_send(
			conn->src_port,
			conn->dst_port,
			conn->dest_ip,
			conn->send_initial_seqnum,
			conn->recv_initial_seqnum,
			TCP_FLAG_SYN,
			NULL,
			0
		);
	}
}

static void _tcp_conn_recv(tcp_conn_t* conn, tcp_packet_t* packet, uint32_t data_len) {
	uint32_t recv_ack_num = ntohl(packet->header.acknowledgement_number);
	uint32_t recv_seq_num = ntohl(packet->header.sequence_number);
	printf("\tTCP: Conn recv, len=%u, recv syn=%u ack=%u\n", data_len, recv_seq_num - conn->recv_initial_seqnum, recv_ack_num - conn->send_initial_seqnum);

	// Did the other end acknowledge something from us?
	if (packet->header.flags & TCP_FLAG_ACK) {
		// TODO(PT): What to do here?
		// TODO(PT): Eventually, remove from the "needs to retransmit" list
		if (recv_ack_num > conn->send_oldest_unacked_seqnum) {
			printf("\tTCP: Bump oldest unacked seqnum to %u\n", recv_ack_num - conn->send_initial_seqnum);
			conn->send_oldest_unacked_seqnum = recv_ack_num;
		}
		else {
			printf("\tTCP: Ignore ack for old seqnum %u\n", recv_ack_num - conn->send_initial_seqnum);
		}
	}

	if (data_len) {
		printf("\tTCP Recv data recv_next_seqnum %u recv_initial_seqnum %u recv_seq_num %u data_len %u\n", conn->recv_next_seqnum, conn->recv_initial_seqnum, recv_seq_num, data_len);
		// Have we already ingested this data?
		if (conn->recv_next_seqnum >= recv_seq_num + data_len) {
			printf("\tTCP: Ignoring recv of already-ingested segment %u\n", recv_seq_num - conn->recv_initial_seqnum);
		}
		else {
			// Send an "ack" for this data
			printf("\tTCP: Sending ACK...\n");
			conn->recv_next_seqnum += data_len;
			tcp_send(
				conn->src_port,
				conn->dst_port,
				conn->dest_ip,
				conn->send_next_seqnum,
				conn->recv_next_seqnum,
				TCP_FLAG_ACK,
				NULL,
				0
			);

			// Add it to the connection's receive buffer
			tcp_segment_t* seg = calloc(1, sizeof(seg));
			seg->buf = calloc(1, data_len);
			seg->len = data_len;
			memcpy(seg->buf, packet->data, data_len);
			array_insert(conn->receive_buffer, seg);
			hexdump(packet->data, data_len);
			printf("\tTCP: Added recv'd segment to receive buffer (new recv buffer size: %d, conn 0x%08x recv buf 0x%08x)\n", conn->receive_buffer->size, conn, conn->receive_buffer);
			// And kick off any ready callback
			callback_list_invoke_ready_callbacks(_tcp_callbacks);
		}
	}
}

void tcp_receive(packet_info_t* packet_info, tcp_packet_t* packet, uint32_t packet_size) {
	uint16_t src_port = ntohs(packet->header.source_port);
	uint16_t dst_port = ntohs(packet->header.dest_port);

	uint32_t packet_header_len = packet->header.header_length * sizeof(uint32_t);
	printf("TCP packet SrcPort[%d] DstPort[%d] Len[%d]\n", src_port, dst_port, packet_header_len);

	// Ignore the packet if it's not intended for this machine
	uint8_t ipv4[IPv4_ADDR_SIZE];
	net_copy_local_ipv4_addr(ipv4);
	if (memcmp(packet_info->dst_ipv4, ipv4, IPv4_ADDR_SIZE)) {
		printf("\tIgnoring packet intended for a different host\n");
		free(packet);
		return;
	}
	uint8_t* data = (uint8_t*)packet + packet_header_len;
	uint32_t data_len = packet_size - packet_header_len;
	printf("\tTCP packet data size: %d\n", data_len);

	// Find the connection awaiting data and dispatch the appropriate event
	for (uint32_t i = 0; i < _tcp_conns->size; i++) {
		tcp_conn_t* conn = array_lookup(_tcp_conns, i);
		if (conn->src_port != dst_port || conn->dst_port != src_port) {
			continue;
		}
		if (memcmp(conn->dest_ip, packet_info->src_ipv4, IPv4_ADDR_SIZE)) {
			//printf("\tNot intended for this connection, conn[%d] is remote %d.%d.%d.%d, packet remote is %d.%d.%d.%d\n", i, conn->dest_ip[0], conn->dest_ip[1], conn->dest_ip[2], conn->dest_ip[3], packet_info->src_ipv4[0], packet_info->src_ipv4[1], packet_info->src_ipv4[2], packet_info->src_ipv4[3]);
			continue;
		}
		// The data is intended for this connection; what's happened?
		if (conn->status == TCP_SYN_SENT) {
			_tcp_handle_syn_response(conn, packet);
		}
		else if (conn->status == TCP_ESTABLISHED) {
			printf("\tReceived data from established connection\n");
			_tcp_conn_recv(conn, packet, data_len);
		}
	}

	free(packet);
}

typedef struct tcp_amc_rpc__conn_open {
	char* amc_service;
	tcp_conn_t* conn;
} tcp_amc_rpc__conn_open_t;

static bool _tcp_is_amc_rpc__conn_open_satisfied(tcp_amc_rpc__conn_open_t* rpc) {
	// Has the connection entered the "established" state?
	printf("_tcp_is_amc_rpc__conn_open_satisfied\n");
	return rpc->conn->status == TCP_ESTABLISHED;
}

static void _tcp_complete_amc_rpc__conn_open(tcp_amc_rpc__conn_open_t* rpc) {
	printf("TCP RPC: Completed conn_open, responding to %s...\n", rpc->amc_service);

	net_message_t msg;
	msg.event = NET_RPC_RESPONSE_TCP_OPEN;
	msg.m.tcp_open_response.tcp_conn_descriptor = array_index(_tcp_conns, rpc->conn);
	amc_message_construct_and_send(rpc->amc_service, &msg, sizeof(msg));

    // Free the buffers we stored
	// Don't touch the tcp_conn_t as it's still alive
    free(rpc->amc_service);
    free(rpc);
}

void tcp_perform_amc_rpc__conn_open(const char* amc_service, net_rpc_tcp_open_t* tcp_open_msg) {
	// Set up a connection structure
	tcp_conn_t* conn = _tcp_conn_alloc(
		tcp_open_msg->src_port,
		tcp_open_msg->dst_port,
		tcp_open_msg->dst_ipv4
	);
	// Set up a callback for when the server sends a response to the handshake open
	tcp_amc_rpc__conn_open_t* cb_info = calloc(1, sizeof(tcp_amc_rpc__conn_open_t));
	cb_info->amc_service = strndup(amc_service, AMC_MAX_SERVICE_NAME_LEN);
	cb_info->conn = conn;
	callback_list_add_callback(
		_tcp_callbacks,
		cb_info,
		(cb_is_satisfied_func)_tcp_is_amc_rpc__conn_open_satisfied,
		(cb_complete_func)_tcp_complete_amc_rpc__conn_open
	);

	// Initiate the TCP handshake
	printf("TCP: Conn open CB set up, init handshake...\n");
	tcp_conn_init(conn);
}

void tcp_perform_amc_rpc__conn_send(const char* amc_service, net_rpc_tcp_send_t* tcp_send_msg) {
	tcp_conn_t* conn = array_lookup(_tcp_conns, tcp_send_msg->tcp_conn_descriptor);
	assert(conn, "Invalid TCP conn descriptor");

	tcp_send(
		conn->src_port,
		conn->dst_port,
		conn->dest_ip,
		conn->send_next_seqnum,
		conn->recv_next_seqnum,
		TCP_FLAG_ACK | TCP_FLAG_PUSH,
		tcp_send_msg->data,
		tcp_send_msg->len
	);
	// TODO(PT): Eventually we need to keep track of what the other end has 
	// received, and resend it
	// For now, assume reliable delivery
	conn->send_next_seqnum += tcp_send_msg->len;
}

typedef struct tcp_amc_rpc__conn_read {
	char* amc_service;
	tcp_conn_t* conn;
	uint32_t len;
} tcp_amc_rpc__conn_read_t;

static bool _tcp_is_amc_rpc__conn_read_satisfied(tcp_amc_rpc__conn_read_t* rpc) {
	// Do we have a segment awaiting processing?
	printf("_tcp_is_amc_rpc__conn_read_satisfied recv buffer size %d conn 0x%08x recv buf 0x%08x\n", rpc->conn->receive_buffer->size, rpc->conn, rpc->conn->receive_buffer);
	for (uint32_t i = 0; i < rpc->conn->receive_buffer->size; i++) {
		tcp_segment_t* seg = array_lookup(rpc->conn->receive_buffer, i);
		printf("Seg->len %u rpc->len %u\n", seg->len, rpc->len);
		assert(seg->len <= rpc->len, "Segment was smaller than RPC buffer");
		// Found a segment we can fulfill the read with!
		printf("Found segment to fulfill TCP read\n");
		return true;
	}
	return false;
}

static void _tcp_complete_amc_rpc__conn_read(tcp_amc_rpc__conn_read_t* rpc) {
	printf("TCP RPC: Completed conn_read, responding to %s conn 0x%08x recv buf 0x%08x...\n", rpc->amc_service, rpc->conn, rpc->conn->receive_buffer);
	for (uint32_t i = 0; i < rpc->conn->receive_buffer->size; i++) {
		tcp_segment_t* seg = array_lookup(rpc->conn->receive_buffer, i);
		printf("copy segment %d\n", i);

		uint32_t msg_size = sizeof(net_message_t) + seg->len;
		net_message_t* msg = calloc(1, msg_size);
		msg->event = NET_RPC_RESPONSE_TCP_READ;
		msg->m.tcp_read_response.tcp_conn_descriptor = array_index(_tcp_conns, rpc->conn);
		msg->m.tcp_read_response.len = seg->len;
		printf("msg tcp_conn_desc %d len %u data 0x%08x\n", msg->m.tcp_read_response.tcp_conn_descriptor, msg->m.tcp_read_response.len, msg->m.tcp_read_response.data);
		memcpy(&msg->m.tcp_read_response.data, seg->buf, seg->len);
		printf("msg buf 0x%08x\n", &msg->m.tcp_read_response.data);
		amc_message_construct_and_send(rpc->amc_service, msg, msg_size);
		free(msg);

		// Free the buffers we stored
		// Don't touch the tcp_conn_t as it's still alive
		array_remove(rpc->conn->receive_buffer, i);
		free(seg->buf);
		free(seg);
		free(rpc->amc_service);
		free(rpc);
		return;
	}
	assert(false, "Failed to find segment to complete read with");
}

void tcp_perform_amc_rpc__conn_read(const char* amc_service, net_rpc_tcp_read_t* tcp_read_msg) {
	tcp_conn_t* conn = array_lookup(_tcp_conns, tcp_read_msg->tcp_conn_descriptor);
	assert(conn, "Invalid TCP conn descriptor");

	// Set up a callback for when we have data to provide to the client
	tcp_amc_rpc__conn_read_t* cb_info = calloc(1, sizeof(tcp_amc_rpc__conn_read_t));
	cb_info->amc_service = strndup(amc_service, AMC_MAX_SERVICE_NAME_LEN);
	cb_info->conn = conn;
	cb_info->len = tcp_read_msg->len;
	callback_list_add_callback(
		_tcp_callbacks,
		cb_info,
		(cb_is_satisfied_func)_tcp_is_amc_rpc__conn_read_satisfied,
		(cb_complete_func)_tcp_complete_amc_rpc__conn_read
	);

	printf("TCP: Conn read CB set up, checking if there is data available now...\n");
	callback_list_invoke_callback_if_ready(_tcp_callbacks, cb_info);
}