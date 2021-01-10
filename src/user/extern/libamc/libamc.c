#include "libamc.h"

// Valid for all message types
const char* amc_message_source(amc_message_t* msg) {
    return (const char*)msg->hdr.source;
}

const char* amc_message_dest(amc_message_t* msg) {
    return (const char*)msg->hdr.dest;
}

// Valid for amc_charlist_message_t
char* amc_charlist_msg_data(amc_charlist_message_t* msg) {
    return (char*)&msg->body.charlist.data;
}

void amc_charlist_msg__set_len(amc_charlist_message_t* msg, uint8_t len) {
    msg->body.charlist.len = len;
}

uint8_t amc_charlist_msg__get_len(amc_charlist_message_t* msg) {
    return msg->body.charlist.len;
}

// Valid for amc_command_message_t
char* amc_command_msg_data(amc_command_message_t* msg) {
    return (char*)&msg->body.cmd.data;
}

void amc_command_msg__set_command(amc_command_message_t* msg, uint32_t command) {
    msg->body.cmd.command = command;
}

uint32_t amc_command_msg__get_command(amc_command_message_t* msg) {
    return msg->body.cmd.command;
}

// Valid for amc_command_ptr_message_t
void amc_command_msg__send(const char* destination, uint32_t command) {
    int buf;
	amc_command_message_t* msg = (amc_command_message_t*)amc_message_construct((const char*)&buf, 1);
	amc_command_msg__set_command(msg, command);
	amc_message_send(destination, msg);
}

char* amc_command_ptr_msg_data(amc_command_ptr_message_t* msg) {
    return (char*)&msg->body.cmd_ptr.data;
}

void amc_command_ptr_msg__set_command(amc_command_ptr_message_t* msg, uint32_t command) {
    msg->body.cmd_ptr.command = command;
}

uint32_t amc_command_ptr_msg__get_command(amc_command_ptr_message_t* msg) {
    return msg->body.cmd_ptr.command;
}

void amc_command_ptr_msg__set_ptr(amc_command_ptr_message_t* msg, uint32_t ptr) {
    msg->body.cmd_ptr.ptr_val = ptr;
}

uint32_t amc_command_ptr_msg__get_ptr(amc_command_ptr_message_t* msg) {
    return msg->body.cmd_ptr.ptr_val;
}

void amc_command_ptr_msg__send(const char* destination, uint32_t command, uint32_t ptr_val) {
	int buf;
	amc_command_ptr_message_t* msg = (amc_command_ptr_message_t*)amc_message_construct((const char*)&buf, 1);
	amc_command_ptr_msg__set_command(msg, command);
	amc_command_ptr_msg__set_ptr(msg, ptr_val);
	amc_message_send(destination, msg);
}
