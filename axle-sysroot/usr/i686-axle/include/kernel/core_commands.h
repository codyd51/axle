#ifndef AMC_CORE_COMMANDS_H
#define AMC_CORE_COMMANDS_H

#include <stdint.h>

#define AXLE_CORE_SERVICE_NAME "com.axle.core"
#define AMC_MAX_SERVICE_NAME_LEN 64

#define AMC_COPY_SERVICES (1 << 0)
#define AMC_COPY_SERVICES_RESPONSE (1 << 0)

typedef struct amc_service_description {
	char service_name[AMC_MAX_SERVICE_NAME_LEN];
	uint32_t unread_message_count;
} amc_service_description_t;

typedef struct amc_service_list {
    uint32_t event;
	uint32_t service_count;
	amc_service_description_t service_descs[];
} amc_service_list_t;

#define AMC_AWM_MAP_FRAMEBUFFER (1 << 1)
#define AMC_AWM_MAP_FRAMEBUFFER_RESPONSE (1 << 1)

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


#define AMC_SLEEP_UNTIL_TIMESTAMP (1 << 2)

#define AMC_FILE_MANAGER_MAP_INITRD (1 << 3)
#define AMC_FILE_MANAGER_MAP_INITRD_RESPONSE (1 << 3)

typedef struct amc_initrd_info {
    uint32_t event;
    uint32_t initrd_start;
    uint32_t initrd_end;
    uint32_t initrd_size;
} amc_initrd_info_t;

#define AMC_FILE_MANAGER_EXEC_BUFFER (1 << 4)
#define AMC_FILE_MANAGER_EXEC_BUFFER_RESPONSE (1 << 4)

typedef struct amc_exec_buffer_cmd {
    uint32_t event;
    const char* program_name;
    void* buffer_addr;
    uint32_t buffer_size;
} amc_exec_buffer_cmd_t;

#define AMC_SHARED_MEMORY_DESTROY (1 << 5)

typedef struct amc_shared_memory_destroy_cmd {
    uint32_t event;
	char remote_service[AMC_MAX_SERVICE_NAME_LEN];
	uint32_t shmem_size;
	uint32_t shmem_local;
	uint32_t shmem_remote;
} amc_shared_memory_destroy_cmd_t;

#define AMC_SYSTEM_PROFILE_REQUEST (1 << 7)
#define AMC_SYSTEM_PROFILE_RESPONSE (1 << 7)

typedef struct amc_system_profile_response {
    uint32_t event;
    uint32_t pmm_allocated;
    uint32_t kheap_allocated;
} amc_system_profile_response_t;

#define AMC_SLEEP_UNTIL_TIMESTAMP_OR_MESSAGE (1 << 8)

void amc_core_handle_message(const char* source_service, void* buf, uint32_t buf_size);

#endif