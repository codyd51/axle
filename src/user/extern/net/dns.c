#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <libport/libport.h>

#include "dns.h"
#include "util.h" // For hexdump, can remove
#include "udp.h"

#define DNS_TYPE_TEXT_STRINGS   16
#define DNS_TYPE_POINTER        12
#define DNS_TYPE_A_RECORD       1

typedef struct dns_question {
    dns_name_parse_state_t parsed_name;
    uint16_t type;
    uint16_t class;
} dns_question_t;

typedef struct dns_answer {
    dns_name_parse_state_t parsed_name;
    uint16_t type;
    uint16_t class;
    uint32_t time_to_live;
    uint16_t data_length;
    uint8_t* data;
} dns_answer_t;

static dns_service_type_t _dns_service_type_table[DNS_SERVICE_TYPE_TABLE_SIZE] = {0};
static dns_domain_t _dns_domain_records[DNS_DOMAIN_RECORDS_TABLE_SIZE] = {0};

static uint8_t _dns_name_read_label_len(uint8_t** data_ptr_in) {
    uint8_t* data_ptr = *data_ptr_in;
    uint8_t label_len = *(data_ptr++);
    // Write the new position of the pointer
    *data_ptr_in = data_ptr;
    return label_len;
}

static void _dns_name_read_label(uint8_t** data_ptr_in, uint8_t label_len, char* out_buf) {
    uint8_t* data_ptr = *data_ptr_in;

    strncpy(out_buf, data_ptr, label_len);
    out_buf[label_len] = '\0';
    data_ptr += label_len;

    // Write the new position of the pointer
    *data_ptr_in = data_ptr;
}

static uint16_t _read_u16(uint8_t** data_ptr_in) {
    uint8_t* data_ptr = *data_ptr_in;

    uint8_t b1 = *(data_ptr++);
    uint8_t b2 = *(data_ptr++);
    uint16_t val = (b1 << 8) | b2;

    // Write the new position of the pointer
    *data_ptr_in = data_ptr;
    return val;
}

static uint32_t _read_u32(uint8_t** data_ptr_in) {
    uint8_t* data_ptr = *data_ptr_in;

    uint8_t b1 = *(data_ptr++);
    uint8_t b2 = *(data_ptr++);
    uint8_t b3 = *(data_ptr++);
    uint8_t b4 = *(data_ptr++);
    uint16_t val = (b1 << 24) | 
                   (b2 << 16) | 
                   (b3 << 8) | 
                   (b4 << 0);

    // Write the new position of the pointer
    *data_ptr_in = data_ptr;
    return val;
}

static void _parse_dns_name(dns_packet_t* packet, dns_name_parse_state_t* out_state, uint8_t** data_ptr_in) {
    memset(out_state, 0, sizeof(dns_name_parse_state_t));

    uint8_t* data_ptr = *data_ptr_in;

    // The DNS body compression scheme allows a name to be represented as:
    // - A sequence of labels ending in a zero byte
    // - A pointer
    // - A sequence of labels ending in a pointer

    // TODO(PT): How to impose an upper limit here?
    while (true) {
        uint8_t label_len = _dns_name_read_label_len(&data_ptr);

        // If the high two bits of the label are set, 
        // this is a pointer to a prior string
        if ((label_len >> 6) == 0x3) {
            out_state->label_count++;

            // Mask off the high two bits
            uint8_t b1 = label_len & ~(3 << 6);
            uint8_t b2 = *(data_ptr)++;
            uint8_t string_offset = (b1 << 8) | b2;

            dns_name_parse_state_t pointer_parse = {0};
            uint8_t* label_offset = (uint8_t*)packet + string_offset;
            _parse_dns_name(packet, &pointer_parse, &label_offset);
            out_state->name_len += snprintf(
                out_state->name + out_state->name_len,
                sizeof(out_state->name),
                "%s",
                pointer_parse.name
            );

            // Pointers are always the end of a name
            break;
        }

        // If we're in a label list and just encountered a null byte, we're done
        if (!label_len) {
            // Trim the final '.' from the name
            out_state->name[--out_state->name_len] = '\0';
            break;
        }
        else {
            // Read a label literal
            char* buf = malloc(label_len+1);
            _dns_name_read_label(&data_ptr, label_len, buf);
            out_state->name_len += snprintf(
                out_state->name + out_state->name_len, 
                sizeof(out_state->name), 
                "%s.",
                buf
            );
            free(buf);
        }
    }

    // Write the new position of the pointer
    *data_ptr_in = data_ptr;
}

static void _parse_dns_question(dns_packet_t* packet, dns_question_t* question, uint8_t** data_ptr_in) {
    memset(question, 0, sizeof(dns_question_t));

    uint8_t* data_ptr = *data_ptr_in;

    dns_name_parse_state_t pointer_parse = {0};
    _parse_dns_name(packet, &question->parsed_name, &data_ptr);

    question->type = _read_u16(&data_ptr);
    question->class = _read_u16(&data_ptr);

    printf("DNS question: %s, type %04x class %04x\n", question->parsed_name.name, question->type, question->class);

    // Write the new position of the pointer
    *data_ptr_in = data_ptr;
}

static dns_service_type_t* _find_or_create_dns_service_type(dns_answer_t* answer) {
    // Did the service type exist already?
    for (int i = 0; i < DNS_SERVICE_TYPE_TABLE_SIZE; i++) {
        dns_service_type_t* ent = &_dns_service_type_table[i];
        if (ent->allocated) {
            if (!strcmp(ent->type_name.name, answer->parsed_name.name)) {
                printf("Found an existing service with name %s\n", answer->parsed_name.name);
                return ent;
            }
        }
    }

    // Create the service type
    for (int i = 0; i < DNS_SERVICE_TYPE_TABLE_SIZE; i++) {
        dns_service_type_t* ent = &_dns_service_type_table[i];
        if (!ent->allocated) {
            ent->allocated = true;
            memcpy(&ent->type_name, &answer->parsed_name, sizeof(dns_name_parse_state_t));
            memset(ent->instances, 0, sizeof(ent->instances));
            printf("Created new DNS service type %s sz %d\n", answer->parsed_name.name, sizeof(ent->instances));
            return ent;
        }
    }

    assert(false, "Failed to find a free slot to add a new DNS service type");
}

static void _update_dns_service_type_with_ptr_record(dns_answer_t* answer, dns_name_parse_state_t* ptr_record) {
    printf("Inserting DNS answer record\n");
    dns_service_type_t* service = _find_or_create_dns_service_type(answer);
    printf("Found service type %s\n", service->type_name.name);

    // Did the service instance exist already?
    for (int i = 0; i < DNS_SERVICE_INSTANCE_TABLE_SIZE; i++) {
        dns_service_instance_t* ent = &service->instances[i];
        if (ent->allocated) {
            printf("Existing record with name: %s, matching %s\n", ent->service_name.name, ptr_record->name);
            if (!strcmp(ent->service_name.name, ptr_record->name)) {
                printf("%s: Found existing INSTANCE with name %s\n", answer->parsed_name.name, ptr_record->name);
                return;
            }
        }
    }
    // Create the service instance
    for (int i = 0; i < DNS_SERVICE_INSTANCE_TABLE_SIZE; i++) {
        dns_service_instance_t* ent = &service->instances[i];
        if (!ent->allocated) {
            ent->allocated = true;
            memcpy(&ent->service_name, ptr_record, sizeof(dns_name_parse_state_t));
            printf("%s: Created new INSTANCE with name %s %s\n", answer->parsed_name.name, ptr_record->name, ent->service_name.name);
            return;
        }
    }
    assert(false, "Failed to find a free slot to add a new DNS service instance");
}

static void _update_domain_name_with_a_record(dns_answer_t* answer) {
    assert(answer->data_length == IPv4_ADDR_SIZE, "A record data size wasn't right");
    char ip_fmt[64] = {0};
    format_ipv4_address__buf(ip_fmt, sizeof(ip_fmt), answer->data);
    printf("DNS: Domain %s @ %s (A record)\n", answer->parsed_name.name, ip_fmt);

    // Check if we already have a mapping for this domain
    for (int i = 0; i < DNS_DOMAIN_RECORDS_TABLE_SIZE; i++) {
        dns_domain_t* ent = &_dns_domain_records[i];
        if (ent->allocated) {
            if (!strcmp(ent->name.name, answer->parsed_name.name)) {
                printf("DNS: Update existing A record for %s\n", ent->name.name);
                memcpy(ent->a_record, answer->data, IPv4_ADDR_SIZE);
                return;
            }
        }
    }

    // New DNS mapping
    for (int i = 0; i < DNS_DOMAIN_RECORDS_TABLE_SIZE; i++) {
        dns_domain_t* ent = &_dns_domain_records[i];
        if (!ent->allocated) {
            printf("DNS: Created new A record for %s\n", answer->parsed_name.name);
            ent->allocated = true;
            memcpy(&ent->name, &answer->parsed_name, sizeof(dns_name_parse_state_t));
            memcpy(ent->a_record, answer->data, IPv4_ADDR_SIZE);
            return;
        }
    }
}

static void _parse_dns_answer(dns_packet_t* packet, dns_answer_t* answer, uint8_t** data_ptr_in) {
    memset(answer, 0, sizeof(dns_answer_t));

    uint8_t* data_ptr = *data_ptr_in;

    _parse_dns_name(packet, &answer->parsed_name, &data_ptr);
    answer->type = _read_u16(&data_ptr);
    answer->class = _read_u16(&data_ptr);

    answer->time_to_live = _read_u32(&data_ptr);
    answer->data_length = _read_u16(&data_ptr);

    answer->data = malloc(answer->data_length);
    memcpy(answer->data, data_ptr, answer->data_length);
    data_ptr += answer->data_length;

    printf("DNS answer: %s, type %04x class %04x ttl %d data_len %d\n", answer->parsed_name.name, answer->type, answer->class, answer->time_to_live, answer->data_length);

    if (answer->type == DNS_TYPE_POINTER) {
        dns_name_parse_state_t pointer_parse = {0};
        uint8_t* ptr_start = answer->data;
        _parse_dns_name(packet, &pointer_parse, &ptr_start);
        printf("\tPointer: %s\n", pointer_parse.name);
        _update_dns_service_type_with_ptr_record(answer, &pointer_parse);
    }
    else if (answer->type == DNS_TYPE_A_RECORD) {
        _update_domain_name_with_a_record(answer);
    }
    else if (answer->type == DNS_TYPE_TEXT_STRINGS) {
        /*
        uint8_t* txt_start = answer->data;
        uint8_t* txt_head = txt_start;
        while (txt_head < txt_start + answer->data_length) {
            dns_name_parse_state_t txt_parse = {0};
            _parse_dns_name(packet, &txt_parse, &txt_head);
            printf("DNS TXT entry %s: %s\n", answer->parsed_name.name, txt_parse.name);
        }
        */
    }
    else {
        //printf("Unknown answer type %d, will hexdump contents\n", answer->type);
        //hexdump(answer->data, answer->data_length);
    }

    // Write the new position of the pointer
    *data_ptr_in = data_ptr;
}

void dns_receive(packet_info_t* packet_info, dns_packet_t* packet, uint32_t packet_size) {
    uint8_t* dns_data = (uint8_t*)&packet->data;
    printf("DNS ID 0x%04x resp %d op %d rcode %d qc %d ac %d authc %d addc %d\n", 
        ntohs(packet->identifier), 
        packet->query_response_flag, 
        packet->opcode, 
        packet->response_code, 
        ntohs(packet->question_count), 
        ntohs(packet->answer_count), 
        ntohs(packet->authority_count), 
        ntohs(packet->additional_record_count)
    );

    uint32_t dns_data_size = packet_size - ((uint8_t*)dns_data - (uint8_t*)packet);

    if (packet->opcode == DNS_OP_QUERY || packet->opcode == DNS_OP_STATUS) {
        uint8_t* data_head = dns_data;
        for (int i = 0; i < ntohs(packet->question_count); i++) {
            dns_question_t parsed_question = {0};
            _parse_dns_question(packet, &parsed_question, &data_head);
        }
        for (int i = 0; i < ntohs(packet->answer_count); i++) {
            dns_answer_t parsed_answer = {0};
            _parse_dns_answer(packet, &parsed_answer, &data_head);
        }
    }
    else if (packet->opcode == DNS_OP_STATUS) {
        //printf("DNS status!\n");
    }

    free(packet);
}

void dns_send(void) {
    dns_packet_t header = {0};
    header.identifier = htons(0x4546);
    /*
    // TODO(PT): Might need to flip the order of this whole bitfield?
    header.query_response_flag = 0;
    header.opcode = 0;
    header.recursion_desired_flag = 1;
    */
    uint16_t* h = (uint16_t*)&header;
    h[1] = htons(0x0100);

    header.question_count = htons(1);

    dns_question_t question;

    char buf[128];
    char* buf_ptr = buf;
    const char* labels[] = {"www", "google", "com", NULL};
    for (int i = 0; i < 3; i++) {
        const char* label = labels[i];
        int len = strlen(label);
        *(buf_ptr++) = len;
        for (int j = 0; j < len; j++) {
            *(buf_ptr++) = label[j];
        }
    }
    *(buf_ptr++) = '\0';
    // Type: A
    *(buf_ptr++) = 0x0;
    *(buf_ptr++) = 0x1;
    // Class: IN
    *(buf_ptr++) = 0x0;
    *(buf_ptr++) = 0x1;
    int buf_len = buf_ptr - buf;

    uint32_t dns_packet_size = sizeof(dns_packet_t) + buf_len;
    char* dns_packet = malloc(dns_packet_size);
    memcpy(dns_packet, &header, sizeof(dns_packet_t));
    memcpy(dns_packet + sizeof(dns_packet_t), buf, buf_len);

    uint8_t router_ip[IPv4_ADDR_SIZE];
    net_copy_router_ipv4_addr(router_ip);
    udp_send(dns_packet, dns_packet_size, 51303, 53, router_ip);
}

dns_service_type_t* dns_service_type_table(void) {
	return _dns_service_type_table;
}

dns_domain_t* dns_domain_records(void) {
	return _dns_domain_records;
}
