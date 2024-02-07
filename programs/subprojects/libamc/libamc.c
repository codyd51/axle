#include <stdio.h>
#include <string.h>

#include "libamc.h"

#include <daemons/watchdogd/watchdogd_messages.h>

// Valid for all message types
const char* amc_message_source(amc_message_t* msg) {
    return (const char*)msg->source;
}

const char* amc_message_dest(amc_message_t* msg) {
    return (const char*)msg->dest;
}

// Convenience constructors

void amc_msg_u32_1__send(const char* destination, uint32_t w1) {
    uint32_t buf[1] = {w1};
    amc_message_send(destination, &buf, sizeof(buf));
}

void amc_msg_u32_2__send(const char* destination, uint32_t w1, uint32_t w2) {
    uint32_t buf[2] = {w1, w2};
    amc_message_send(destination, &buf, sizeof(buf));
}

void amc_msg_u32_3__send(const char* destination, uint32_t w1, uint32_t w2, uint32_t w3) {
    uint32_t buf[3] = {w1, w2, w3};
    amc_message_send(destination, &buf, sizeof(buf));
}

void amc_msg_u32_5__send(const char* destination, uint32_t w1, uint32_t w2, uint32_t w3, uint32_t w4, uint32_t w5) {
    uint32_t buf[5] = {w1, w2, w3, w4, w5};
    amc_message_send(destination, &buf, sizeof(buf));
}

void amc_msg_u32_6__send(const char* destination, uint32_t w1, uint32_t w2, uint32_t w3, uint32_t w4, uint32_t w5, uint32_t w6) {
    uint32_t buf[6] = {w1, w2, w3, w4, w5, w6};
    amc_message_send(destination, &buf, sizeof(buf));
}

void amc_msg_uptr_1__send(const char* destination, uintptr_t w1) {
    uintptr_t buf[1] = {w1};
    amc_message_send(destination, &buf, sizeof(buf));
}

void amc_msg_uptr_2__send(const char* destination, uintptr_t w1, uintptr_t w2) {
    uintptr_t buf[2] = {w1, w2};
    amc_message_send(destination, &buf, sizeof(buf));
}

void amc_msg_uptr_3__send(const char* destination, uintptr_t w1, uintptr_t w2, uintptr_t w3) {
    uintptr_t buf[3] = {w1, w2, w3};
    amc_message_send(destination, &buf, sizeof(buf));
}

void amc_msg_i8_3__send(const char* destination, int8_t b1, int8_t b2, int8_t b3) {
    int8_t buf[3] = {b1, b2, b3};
    amc_message_send(destination, &buf, sizeof(buf));
}

void amc_msg_i8_4__send(const char* destination, int8_t b1, int8_t b2, int8_t b3, int8_t b4) {
    int8_t buf[4] = {b1, b2, b3, b4};
    amc_message_send(destination, &buf, sizeof(buf));
}

uint32_t amc_msg_u32_get_word(amc_message_t* msg, uint32_t word_idx) {
    uint32_t* buf = (uint32_t*)msg->body;
    return buf[word_idx];
}

uintptr_t amc_msg_uptr_get(amc_message_t* msg, uint32_t pointer_idx) {
    uintptr_t* buf = (uintptr_t*)msg->body;
    return buf[pointer_idx];
}

void amc_msg_u32_4__request_response_sync(
    amc_message_t** recv_out,
    const char* destination, 
    uint32_t request, 
    uint32_t response, 
    uint32_t w1, 
    uint32_t w2, 
    uint32_t w3, 
    uint32_t w4
) {
    amc_msg_u32_5__send(destination, request, w1, w2, w3, w4);
	// TODO(PT): Should loop until the message is the desired one, discarding others
	amc_message_await(destination, recv_out);
	if (amc_msg_u32_get_word(*recv_out, 0) != response) {
		printf("Invalid state. Expected response 0x%08x\n", response);
        // TODO(PT): Implement assert library and throw one here
	}
}

void amc_msg_u32_5__request_response_sync(
    amc_message_t** recv_out,
    const char* destination, 
    uint32_t request, 
    uint32_t response, 
    uint32_t w1, 
    uint32_t w2, 
    uint32_t w3, 
    uint32_t w4,
    uint32_t w5
) {
    amc_msg_u32_6__send(destination, request, w1, w2, w3, w4, w5);
	// TODO(PT): Should loop until the message is the desired one, discarding others
	amc_message_await(destination, recv_out);
	if (amc_msg_u32_get_word(*recv_out, 0) != response) {
		printf("Invalid state. Expected response 0x%08x\n", response);
        // TODO(PT): Implement assert library and throw one here
	}
}

bool libamc_handle_message(amc_message_t* msg) {
    if (!strncmp(msg->source, WATCHDOGD_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
        if (amc_msg_u32_get_word(msg, 0) == WATCHDOGD_LIVELINESS_PING) {
            printf("libamc responding to liveliness check!\n");
            amc_msg_u32_1__send(msg->source, WATCHDOGD_LIVELINESS_PING_RESPONSE);
            return true;
        }
    }
    return false;
}

void amc_alloc_physical_range(uintptr_t buffer_size, uintptr_t* out_phys_base, uintptr_t* out_virt_base) {
    printf("amc_alloc_physical_range(buffer_size=%p, out_phys=%p, out_virt=%p)\n", (void*)buffer_size, out_phys_base, out_virt_base);
    assert(out_phys_base && out_virt_base, "out-parameters must be provided");
    amc_alloc_physical_range_request_t req = {
        .event = AMC_ALLOC_PHYSICAL_RANGE_REQUEST,
        .size = buffer_size
    };
    amc_message_send(AXLE_CORE_SERVICE_NAME, &req, sizeof(req));
    amc_message_t* out_resp;
    amc_message_await__u32_event(AXLE_CORE_SERVICE_NAME, AMC_ALLOC_PHYSICAL_RANGE_RESPONSE, &out_resp);
    amc_alloc_physical_range_response_t* phys_range_info = (amc_alloc_physical_range_response_t*)out_resp->body;
    *out_phys_base = phys_range_info->phys_base;
    *out_virt_base = phys_range_info->virt_base;
}
