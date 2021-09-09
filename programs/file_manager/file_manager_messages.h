#ifndef FILE_MANAGER_MESSAGES_H
#define FILE_MANAGER_MESSAGES_H

#include <kernel/amc.h>

#define FILE_MANAGER_SERVICE_NAME "com.axle.file_manager"

// Sent from clients to the file manager
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

// Sent from file manager to awm to signal that it's ready to receive file requests
// Prior to this, awm should not request image resources, or else the request may 
// conflict with the file manager's attempts to create a window.
// TODO(PT): Think about ways to prevent this. Perhaps amc messages can be popped with a predicate
#define FILE_MANAGER_READY  102

#define FILE_MANAGER_LAUNCH_FILE 103
typedef struct file_manager_launch_file_request {
    uint32_t event; // FILE_MANAGER_LAUNCH_FILE
    char path[128];
} file_manager_launch_file_request_t;

#endif
