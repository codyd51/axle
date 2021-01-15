#ifndef AMC_H
#define AMC_H

#include <stdbool.h>
#include <stdint.h>

#define AMC_MESSAGE_STRUCT_SIZE     64
#define AMC_MESSAGE_PAYLOAD_SIZE    (AMC_MESSAGE_STRUCT_SIZE - sizeof(amc_msg_header_t))
// This #define is modified from minix/minix/include/minix/ipcconst.h
#define ASSERT_AMC_MSG_BODY_SIZE(msg_type) \
    typedef int _ASSERT_##msg_type[sizeof(msg_type) == AMC_MESSAGE_PAYLOAD_SIZE ? 1 : -1]

typedef struct amc_msg_header {
    const char* source;
    const char* dest; // May be null if the message is globally broadcast
} amc_msg_header_t;

typedef struct amc_msg_body_charlist {
    // 1 byte is safe because the buffer will contain < 256 bytes
    uint8_t len;
    // Subtract the size of the `len` field from the usable size
    char data[AMC_MESSAGE_PAYLOAD_SIZE-sizeof(uint8_t)];
} amc_msg_body_charlist_t;
ASSERT_AMC_MSG_BODY_SIZE(amc_msg_body_charlist_t);

typedef struct amc_msg_body_command {
    uint32_t command;
    char data[AMC_MESSAGE_PAYLOAD_SIZE-sizeof(uint32_t)];
} amc_msg_body_command_t;
ASSERT_AMC_MSG_BODY_SIZE(amc_msg_body_command_t);

typedef struct amc_msg_body_command_ptr {
    uint32_t command;
    uint32_t ptr_val;
    char data[AMC_MESSAGE_PAYLOAD_SIZE-(sizeof(uint32_t) * 2)];
} amc_msg_body_command_ptr_t;
ASSERT_AMC_MSG_BODY_SIZE(amc_msg_body_command_ptr_t);

typedef union amc_msg_body {
    amc_msg_body_charlist_t charlist;
    amc_msg_body_command_t cmd;
    amc_msg_body_command_ptr_t cmd_ptr;
} amc_msg_body_t;
ASSERT_AMC_MSG_BODY_SIZE(amc_msg_body_t);

typedef struct amc_message {
    amc_msg_header_t hdr;
    amc_msg_body_t body;
} amc_message_t;

typedef amc_message_t amc_charlist_message_t;
typedef amc_message_t amc_command_message_t;
typedef amc_message_t amc_command_ptr_message_t;

// Register the running process as the provided service name
void amc_register_service(const char* name);

// Construct an amc message
amc_message_t* amc_message_construct(const char* data, int len);

// Asynchronously send the message to the provided destination service
// Returns whether the message was successfully routed to the service
bool amc_message_send(const char* destination_service, amc_message_t* msg);

// Asynchronously send the message to any service awaiting a message from this service
void amc_message_broadcast(amc_message_t* msg);

// Block until a message has been received from the source service
void amc_message_await(const char* source_service, amc_message_t* out);
// Block until a message has been received from any of the provided source services
void amc_message_await_from_services(int source_service_count, const char** source_services, amc_message_t* out);
// Await a message from any service
// Blocks until a message is received
void amc_message_await_any(amc_message_t* out);

// Returns whether the service has a message in its inbox from the provided service
// The return value indicates whether a call to `amc_message_await` is currently non-blocking
bool amc_has_message_from(const char* source_service);
// Returns whether the service has any message in its inbox
// The return value indicates whether a call to `amc_message_await` is currently non-blocking
bool amc_has_message(void);

// Launch an amc service with a known name.
// Returns whether the service was successfully found and launched.
bool amc_launch_service(const char* service_name);

// Create a shared memory region between the current service and a destination service
// Writes the virtual addresses of the local and remote regions to the "out" parameters
// Both virtual memory regions point to the same physical memory
void amc_shared_memory_create(const char* remote_service, uint32_t buffer_size, uint32_t* local_buffer, uint32_t* remote_buffer);

// #############
// Kernel use only
// #############

// Allows syscalls to send messaages reported as originating from "com.axle.core" 
// instead of the process that initiated the syscall
amc_message_t* amc_message_construct__from_core(const char* data, int len);

#endif