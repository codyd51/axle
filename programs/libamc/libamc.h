#ifndef LIBAMC_H
#define LIBAMC_H

#include <kernel/amc.h>

// Valid for all message types
const char* amc_message_source(amc_message_t* msg);
const char* amc_message_dest(amc_message_t* msg);

// Convenience constructors
void amc_msg_u32_1__send(const char* destination, uint32_t w1);
void amc_msg_u32_2__send(const char* destination, uint32_t w1, uint32_t w2);
void amc_msg_u32_3__send(const char* destination, uint32_t w1, uint32_t w2, uint32_t w3);
void amc_msg_u32_5__send(const char* destination, uint32_t w1, uint32_t w2, uint32_t w3, uint32_t w4, uint32_t w5);
void amc_msg_u32_6__send(const char* destination, uint32_t w1, uint32_t w2, uint32_t w3, uint32_t w4, uint32_t w5, uint32_t w6);

void amc_msg_uptr_1__send(const char* destination, uintptr_t w1);
void amc_msg_uptr_2__send(const char* destination, uintptr_t w1, uintptr_t w2);
void amc_msg_uptr_3__send(const char* destination, uintptr_t w1, uintptr_t w2, uintptr_t w3);

void amc_msg_i8_3__send(const char* destination, int8_t b1, int8_t b2, int8_t b3);
void amc_msg_i8_4__send(const char* destination, int8_t b1, int8_t b2, int8_t b3, int8_t b4);

uint32_t amc_msg_u32_get_word(amc_message_t* msg, uint32_t word_idx);

uintptr_t amc_msg_uptr_get(amc_message_t* msg, uint32_t pointer_idx);

// Convenience synchronous construct + send + await reply
// Loops until the desired message arrives, discarding anything else that's received
void amc_msg_u32_4__request_response_sync(
    amc_message_t** recv_out,
    const char* destination, 
    uint32_t request, 
    uint32_t response, 
    uint32_t w1, 
    uint32_t w2, 
    uint32_t w3, 
    uint32_t w4
);

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
);

bool libamc_handle_message(amc_message_t* msg);

#endif