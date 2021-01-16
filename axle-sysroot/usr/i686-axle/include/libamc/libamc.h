#ifndef LIBAMC_H
#define LIBAMC_H

#include <kernel/amc.h>

// Valid for all message types
const char* amc_message_source(amc_message_t* msg);
const char* amc_message_dest(amc_message_t* msg);

// Convenience constructors
// TODO(PT): Check the buffer size to see how many u32 words we can send at once
void amc_msg_u32_1__send(const char* destination, uint32_t w1);
void amc_msg_u32_2__send(const char* destination, uint32_t w1, uint32_t w2);
void amc_msg_u32_3__send(const char* destination, uint32_t w1, uint32_t w2, uint32_t w3);
void amc_msg_u32_5__send(const char* destination, uint32_t w1, uint32_t w2, uint32_t w3, uint32_t w4, uint32_t w5);
void amc_msg_u32_6__send(const char* destination, uint32_t w1, uint32_t w2, uint32_t w3, uint32_t w4, uint32_t w5, uint32_t w6);

// Convenience accessor
uint32_t amc_msg_u32_get_word(amc_command_message_t* msg, uint32_t word_idx);

// Valid for amc_charlist_message_t
char* amc_charlist_msg_data(amc_charlist_message_t* msg);

void amc_charlist_msg__set_len(amc_charlist_message_t* msg, uint8_t len);
uint8_t amc_charlist_msg__get_len(amc_charlist_message_t* msg);

// Valid for amc_command_message_t
void amc_command_msg__send(const char* destination, uint32_t command);
char* amc_command_msg_data(amc_command_message_t* msg);

void amc_command_msg__set_command(amc_command_message_t* msg, uint32_t command);
uint32_t amc_command_msg__get_command(amc_command_message_t* msg);

// Valid for amc_command_ptr_message_t
void amc_command_ptr_msg__send(const char* destination, uint32_t command, uint32_t ptr_val);
char* amc_command_ptr_msg_data(amc_command_ptr_message_t* msg);

void amc_command_ptr_msg__set_command(amc_command_ptr_message_t* msg, uint32_t command);
uint32_t amc_command_ptr_msg__get_command(amc_command_ptr_message_t* msg);

void amc_command_ptr_msg__set_ptr(amc_command_ptr_message_t* msg, uint32_t ptr);
uint32_t amc_command_ptr_msg__get_ptr(amc_command_ptr_message_t* msg);

#endif