#include <stdio.h>
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

// Convenience constructors

void amc_msg_u32_1__send(const char* destination, uint32_t w1) {
    int nil;
    amc_command_message_t* msg = (amc_command_message_t*)amc_message_construct((const char*)&nil, 1);
    amc_command_msg__set_command(msg, w1);
	amc_message_send(destination, msg);
}

void amc_msg_u32_2__send(const char* destination, uint32_t w1, uint32_t w2) {
    int nil;
    amc_command_message_t* msg = (amc_command_message_t*)amc_message_construct((const char*)&nil, 1);
    amc_command_msg__set_command(msg, w1);
    uint32_t* buf = (uint32_t*)amc_command_msg_data(msg);
    buf[0] = w2;
	amc_message_send(destination, msg);
}

void amc_msg_u32_3__send(const char* destination, uint32_t w1, uint32_t w2, uint32_t w3) {
    int nil;
    amc_command_message_t* msg = (amc_command_message_t*)amc_message_construct((const char*)&nil, 1);
    amc_command_msg__set_command(msg, w1);
    uint32_t* buf = (uint32_t*)amc_command_msg_data(msg);
    buf[0] = w2;
    buf[1] = w3;
	amc_message_send(destination, msg);
}

void amc_msg_u32_5__send(const char* destination, uint32_t w1, uint32_t w2, uint32_t w3, uint32_t w4, uint32_t w5) {
    int nil;
    amc_command_message_t* msg = (amc_command_message_t*)amc_message_construct((const char*)&nil, 1);
    amc_command_msg__set_command(msg, w1);
    uint32_t* buf = (uint32_t*)amc_command_msg_data(msg);
    buf[0] = w2;
    buf[1] = w3;
    buf[2] = w4;
    buf[3] = w5;
	amc_message_send(destination, msg);
}

void amc_msg_u32_6__send(const char* destination, uint32_t w1, uint32_t w2, uint32_t w3, uint32_t w4, uint32_t w5, uint32_t w6) {
    int nil;
    amc_command_message_t* msg = (amc_command_message_t*)amc_message_construct((const char*)&nil, 1);
    amc_command_msg__set_command(msg, w1);
    uint32_t* buf = (uint32_t*)amc_command_msg_data(msg);
    buf[0] = w2;
    buf[1] = w3;
    buf[2] = w4;
    buf[3] = w5;
    buf[4] = w6;
	amc_message_send(destination, msg);
}

uint32_t amc_msg_u32_get_word(amc_command_message_t* msg, uint32_t word_idx) {
    if (word_idx == 0) {
        return amc_command_msg__get_command(msg);
    }
    uint32_t* buf = (uint32_t*)amc_command_msg_data(msg);
    return buf[word_idx-1];
}

void amc_msg_u32_4__request_response_sync(
    amc_message_t* recv_out,
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
	if (amc_msg_u32_get_word(recv_out, 0) != response) {
		printf("Invalid state. Expected response 0x%08x\n", response);
        // TODO(PT): Implement assert library and throw one here
	}
}

void amc_msg_u32_5__request_response_sync(
    amc_message_t* recv_out,
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
	if (amc_msg_u32_get_word(recv_out, 0) != response) {
		printf("Invalid state. Expected response 0x%08x\n", response);
        // TODO(PT): Implement assert library and throw one here
	}
}
