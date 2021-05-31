#ifndef FILE_MANAGER_MESSAGES_H
#define FILE_MANAGER_MESSAGES_H

#include <kernel/amc.h>

#define FILE_MANAGER_SERVICE_NAME "com.axle.file_manager"

// Sent from clients to the file manager
// Value chosen to not conflict with awm event values
#define FILE_MANAGER_READ_FILE 100
typedef struct file_manager_read_file_request {
    uint32_t event; // FILE_MANAGER_READ_FILE 
    char path[128];
} file_manager_read_file_request_t;

// Sent from the file manager to clients
#define FILE_MANAGER_READ_FILE_RESPONSE 101
typedef struct file_manager_read_file_response {
    uint32_t event; // FILE_MANAGER_READ_FILE_RESPONSE
    uint32_t file_size;
    uint8_t file_data[];
} file_manager_read_file_response_t;

#endif
