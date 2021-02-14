#ifndef NET_DNS_H
#define NET_DNS_H

#include <stdint.h>
#include "net.h"

#define DNS_OP_QUERY 0x0
#define DNS_OP_STATUS 0x2

#define DNS_SERVICE_TYPE_TABLE_SIZE 32
#define DNS_SERVICE_INSTANCE_TABLE_SIZE 32

typedef struct dns_packet {
	uint16_t identifier;

	uint16_t query_response_flag:1;
	uint16_t opcode:4;
	uint16_t authoritative_answer_flag:1;
	uint16_t truncation_flag:1;
	uint16_t recursion_desired_flag:1;
	uint16_t recursion_available_flag:1;
	uint16_t zero:3;
	uint16_t response_code:4;

	uint16_t question_count;
	uint16_t answer_count;
	uint16_t authority_count;
	uint16_t additional_record_count;

    uint8_t data[];
} __attribute__((packed)) dns_packet_t;

typedef struct dns_name_parse_state {
    // The number of '.'-separated components
    int label_count;
    // Current name length
    uint8_t name_len;
    // Upper bound on length of a name
    char name[256]; 
} dns_name_parse_state_t;

typedef struct dns_service_instance {
    bool allocated;
    dns_name_parse_state_t service_name;
} dns_service_instance_t;

typedef struct dns_service_type {
    bool allocated;
    dns_name_parse_state_t type_name;
    dns_service_instance_t instances[DNS_SERVICE_INSTANCE_TABLE_SIZE];
} dns_service_type_t;

void dns_receive(packet_info_t* packet_info, dns_packet_t* packet, uint32_t packet_size);

dns_service_type_t* dns_service_type_table(void);

#endif