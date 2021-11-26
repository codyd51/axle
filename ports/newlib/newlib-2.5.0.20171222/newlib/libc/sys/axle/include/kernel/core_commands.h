#ifndef AMC_CORE_COMMANDS_H
#define AMC_CORE_COMMANDS_H

#include <stdint.h>

#define AXLE_CORE_SERVICE_NAME "com.axle.core"
#define AMC_MAX_SERVICE_NAME_LEN 64

#define AMC_COPY_SERVICES 200
#define AMC_COPY_SERVICES_RESPONSE 200

typedef struct amc_service_description {
	char service_name[AMC_MAX_SERVICE_NAME_LEN];
	uint32_t unread_message_count;
} amc_service_description_t;

typedef struct amc_service_list {
    uint32_t event;
	uint32_t service_count;
	amc_service_description_t service_descs[];
} amc_service_list_t;

#define AMC_AWM_MAP_FRAMEBUFFER 201
#define AMC_AWM_MAP_FRAMEBUFFER_RESPONSE 201

typedef struct amc_framebuffer_info {
    uint32_t event;
    uint32_t type;
    uintptr_t address;
    uint32_t width;
    uint32_t height;
    uint32_t bits_per_pixel;
    uint32_t bytes_per_pixel;
    uint32_t size;
} amc_framebuffer_info_t;

#define AMC_SLEEP_UNTIL_TIMESTAMP 202

#define AMC_FILE_MANAGER_MAP_INITRD 203
#define AMC_FILE_MANAGER_MAP_INITRD_RESPONSE 203

typedef struct amc_initrd_info {
    uint32_t event;
    uint64_t initrd_start;
    uint64_t initrd_end;
    uint64_t initrd_size;
} amc_initrd_info_t;

#define AMC_FILE_MANAGER_EXEC_BUFFER 204
#define AMC_FILE_MANAGER_EXEC_BUFFER_RESPONSE 204

typedef struct amc_exec_buffer_cmd {
    uint32_t event;
    const char* program_name;
    void* buffer_addr;
    uint32_t buffer_size;
} amc_exec_buffer_cmd_t;

#define AMC_SHARED_MEMORY_DESTROY 205

typedef struct amc_shared_memory_destroy_cmd {
    uint32_t event;
	char remote_service[AMC_MAX_SERVICE_NAME_LEN];
	uint32_t shmem_size;
	uint32_t shmem_local;
	uint32_t shmem_remote;
} amc_shared_memory_destroy_cmd_t;

#define AMC_SYSTEM_PROFILE_REQUEST 206
#define AMC_SYSTEM_PROFILE_RESPONSE 206

typedef struct amc_system_profile_response {
    uint32_t event;
    uint32_t pmm_allocated;
    uint32_t kheap_allocated;
} amc_system_profile_response_t;

#define AMC_SLEEP_UNTIL_TIMESTAMP_OR_MESSAGE 207

#define AMC_REGISTER_NOTIFICATION_SERVICE_DIED 208
#define AMC_SERVICE_DIED_NOTIFICATION 208

typedef struct amc_notify_when_service_dies_cmd {
    uint32_t event; // AMC_REGISTER_NOTIFICATION_SERVICE_DIED
    char remote_service[AMC_MAX_SERVICE_NAME_LEN];
} amc_notify_when_service_dies_cmd_t;

typedef struct amc_service_died_notification {
    uint32_t event; // AMC_SERVICE_DIED_NOTIFICATION
    char dead_service[AMC_MAX_SERVICE_NAME_LEN];
} amc_service_died_notification_t;

#define AMC_FLUSH_MESSAGES_TO_SERVICE 209

typedef struct amc_flush_messages_to_service_cmd {
    uint32_t event; // AMC_FLUSH_MESSAGES_TO_SERVICE
    char remote_service[AMC_MAX_SERVICE_NAME_LEN];
} amc_flush_messages_to_service_cmd_t;

// Create a shared memory region between the current service and a destination service
// Writes the virtual addresses of the local and remote regions to the "out" parameters
// Both virtual memory regions point to the same physical memory
#define AMC_SHARED_MEMORY_CREATE_REQUEST 210
#define AMC_SHARED_MEMORY_CREATE_RESPONSE 210

typedef struct amc_shared_memory_create_cmd {
    uint32_t event;
    char remote_service_name[AMC_MAX_SERVICE_NAME_LEN];
    uint32_t buffer_size;
} amc_shared_memory_create_cmd_t;

typedef struct amc_shared_memory_create_response {
    uint32_t event;
    uintptr_t local_buffer_start;
    uintptr_t remote_buffer_start;
} amc_shared_memory_create_response_t;

void amc_core_handle_message(const char* source_service, void* buf, uint32_t buf_size);

#endif