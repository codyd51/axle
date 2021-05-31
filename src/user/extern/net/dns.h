#ifndef NET_DNS_H
#define NET_DNS_H

#include <stdint.h>
#include "net.h"

#define DNS_OP_QUERY 0x0
#define DNS_OP_STATUS 0x2

#define DNS_SERVICE_TYPE_TABLE_SIZE 64
#define DNS_SERVICE_INSTANCE_TABLE_SIZE 64
#define DNS_DOMAIN_RECORDS_TABLE_SIZE 64

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

typedef struct dns_domain {
	bool allocated;
	dns_name_parse_state_t name;
	uint8_t a_record[IPv4_ADDR_SIZE];
} dns_domain_t;

void dns_init(void);

void dns_receive(packet_info_t* packet_info, dns_packet_t* packet, uint32_t packet_size);
void dns_send(void);

dns_service_type_t* dns_service_type_table(void);
dns_domain_t* dns_domain_records(void);

bool dns_cache_contains_domain(char* domain_name, uint32_t domain_name_len);
bool dns_copy_a_record(char* domain_name, uint32_t domain_name_len, uint8_t out_ipv4[IPv4_ADDR_SIZE]);

void dns_perform_amc_rpc__discover_ipv4(const char* source_service, char* domain_name, uint32_t domain_name_len);

#endif