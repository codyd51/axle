#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Port IO
#include <libport/libport.h>

#include "udp.h"
#include "ipv4.h"
#include "dns.h"

#define UDP_DNS_PORT    53
#define UDP_MDNS_PORT   5353

const uint8_t MDNS_DESTINATION_IP[4] = {224, 0, 0, 251};

void udp_receive(udp_packet_t* packet, uint32_t packet_size, uint32_t source_ip, uint32_t dest_ip) {
	printf("UDP packet SrcPort[%d] DstPort[%d] Len[%d]\n", ntohs(packet->source_port), ntohs(packet->dest_port), ntohs(packet->length));

	if (ip_equals__buf_u32(MDNS_DESTINATION_IP, dest_ip)) {
		//printf("MDNS!!!\n");
	}

	if (ntohs(packet->dest_port) == UDP_DNS_PORT || ntohs(packet->dest_port) == UDP_MDNS_PORT) {
		dns_header_t* dns_header = (dns_header_t*)&packet->data;
		uint8_t* dns_data = (uint8_t*)(dns_header + 1);
		/*
		printf("DNS ID 0x%04x\n resp %d op %d rcode %d qc %d ac %d authc %d addc %d\n", 
			ntohs(dns_header->identifier), 
			dns_header->query_response_flag, 
			dns_header->opcode, 
			dns_header->response_code, 
			ntohs(dns_header->question_count), 
			ntohs(dns_header->answer_count), 
			ntohs(dns_header->authority_count), 
			ntohs(dns_header->additional_record_count)
		);
		*/

		uint32_t dns_data_size = ntohs(packet->length) - ((uint8_t*)dns_data - (uint8_t*)dns_header);
		//printf("DNS len %d %d\n", dns_data_size, ((uint32_t)dns_data - (uint32_t)dns_header));
		//hexdump(dns_data, dns_data_size);

		if (dns_header->opcode == DNS_OP_QUERY) {
			char buf[dns_data_size];
			char* buf_head = buf;
			for (int i = 0; i < ntohs(dns_header->question_count); i++) {
				//printf("Process DNS question %d\n", i);
				while (true) {
					// First, get the number of bytes in the label
					uint8_t label_len = *(dns_data++);
					//printf("Read DNS label of len %d\n", label_len);
					// Did we reach the end of the name field?
					if (label_len == 0) {
						// TODO(PT): Parse QTYPE after refactoring DNS
						// uint16_t
						*(dns_data++);
						*(dns_data++);
						// QCLASS uint16_t
						*(dns_data++);
						*(dns_data++);

						*(buf_head++) = 0;
						printf("Read DNS label %s\n", buf);
						break;
					}
					if (label_len > dns_data_size) {
						// DNS uses string interning - domain name ptr QTYPE
						printf("Unhandled DNS format, will continue\n");
						// memory leak, dnt return
						return;
						break;
					}
					//assert(label_len <= dns_data_size, "Malformed DNS question");

					strncpy(buf_head, dns_data, label_len);
					buf_head += label_len;
					*(buf_head++) = '.';
					dns_data += label_len;
				}
			}
		}
		else if (dns_header->opcode == DNS_OP_STATUS) {
			//printf("DNS status!\n");
		}
	}

	free(packet);
}
