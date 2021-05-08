#ifndef AMC_H
#define AMC_H

#include <stdbool.h>
#include <stdint.h>

#define AMC_MAX_MESSAGE_SIZE 2048
#define AMC_MAX_SERVICE_NAME_LEN 64
typedef struct amc_message_t {
    const char source[AMC_MAX_SERVICE_NAME_LEN];
    const char dest[AMC_MAX_SERVICE_NAME_LEN]; // May be null if the message is globally broadcast
    uint32_t len;
    uint8_t body[];
} amc_message_t;

typedef struct amc_service_description {
	char service_name[AMC_MAX_SERVICE_NAME_LEN];
	uint32_t unread_message_count;
} amc_service_description_t;

typedef struct amc_service_list {
    uint32_t event;
	uint32_t service_count;
	amc_service_description_t service_descs[];
} amc_service_list_t;

typedef struct amc_framebuffer_info {
    uint32_t event;
    // Must match layout of <kernel/boot_info.h>::framebuffer_info_t
    uint32_t type;
    uint32_t address;
    uint32_t width;
    uint32_t height;
    uint32_t bits_per_pixel;
    uint32_t bytes_per_pixel;
    uint32_t size;
} amc_framebuffer_info_t;

typedef struct amc_initrd_info {
    uint32_t event;
    uint32_t initrd_start;
    uint32_t initrd_end;
    uint32_t initrd_size;
} amc_initrd_info_t;

typedef struct amc_exec_buffer_cmd {
    uint32_t event;
    const char* program_name;
    void* buffer_addr;
    uint32_t buffer_size;
} amc_exec_buffer_cmd_t;

#define AXLE_CORE_SERVICE_NAME "com.axle.core"
#define AMC_COPY_SERVICES (1 << 0)
#define AMC_COPY_SERVICES_RESPONSE (1 << 0)

#define AMC_AWM_MAP_FRAMEBUFFER (1 << 1)
#define AMC_AWM_MAP_FRAMEBUFFER_RESPONSE (1 << 1)

#define AMC_TIMED_AWAIT_TIMESTAMP_OR_MESSAGE (1 << 2)

#define AMC_FILE_MANAGER_MAP_INITRD (1 << 3)
#define AMC_FILE_MANAGER_MAP_INITRD_RESPONSE (1 << 3)

#define AMC_FILE_MANAGER_EXEC_BUFFER (1 << 4)
#define AMC_FILE_MANAGER_EXEC_BUFFER_RESPONSE (1 << 4)

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
void amc_message_await(const char* source_service, amc_message_t** out);
// Block until a message has been received from any of the provided source services
void amc_message_await_from_services(int source_service_count, const char** source_services, amc_message_t** out);
// Await a message from any service
// Blocks until a message is received
void amc_message_await_any(amc_message_t** out);

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
void amc_physical_memory_region_create(uint32_t region_size, uint32_t* virtual_region_start_out, uint32_t* physical_region_start_out);

bool amc_message_construct_and_send(const char* destination_service, void* buf, uint32_t buf_size);

// #############
// Kernel use only
// #############

// Allows syscalls to send messages reported as originating from "com.axle.core" 
// instead of the process that initiated the syscall
amc_message_t* amc_message_construct__from_core(const char* data, int len);
bool amc_message_construct_and_send__from_core(const char* destination_service, void* buf, uint32_t buf_size);

bool amc_service_has_message(void* service);

void amc_wake_timed_if_timestamp_reached(void);

#endif