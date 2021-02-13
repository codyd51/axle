#ifndef NET_DNS_H
#define NET_DNS_H

#include <stdint.h>

#define DNS_OP_QUERY 0x0
#define DNS_OP_STATUS 0x2

typedef struct dns_header {
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
} dns_header_t;

#endif