#ifndef LIBAMC_H
#define LIBAMC_H

#include <kernel/amc.h>

// Valid for all message types
const char* amc_message_source(amc_message_t* msg);
const char* amc_message_dest(amc_message_t* msg);

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