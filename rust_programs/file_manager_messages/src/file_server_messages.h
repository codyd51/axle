#ifndef FILE_SERVER_MESSAGES_H
#define FILE_SERVER_MESSAGES_H

#include <stdint.h>

// PT: Add more definitions here as C clients need them

#define FILE_SERVER_SERVICE_NAME "com.axle.file_server"

#define FILE_SERVER_READ_FILE_EVENT 100
typedef struct file_server_read {
    uint32_t event;
    char path[64];
} file_server_read_t;

typedef struct file_server_read_response {
    uint32_t event;
    char path[64];
    uintptr_t len;
    uint8_t data[];
} file_server_read_response_t;

#define FILE_SERVER_LAUNCH_PROGRAM 102
typedef struct file_server_launch_program {
    uint32_t event;
    char path[64];
} file_server_launch_program_t;

#define FILE_SERVER_CHECK_FILE_EXISTS 103
typedef struct file_server_check_file_exists {
    uint32_t event;
    char path[64];
} file_server_check_file_exists_t;

typedef struct file_server_check_file_exists_response {
    uint32_t event;
    char path[64];
    bool exists;
    uintptr_t file_size;
} file_server_check_file_exists_response_t;

// Sent from clients to the file server
#define FILE_SERVER_READ_FILE__PARTIAL 104
typedef struct file_SERVER_read_file_partial_request {
    uint32_t event;
    char path[64];
    uintptr_t offset;
    uintptr_t length;
} file_server_read_file_partial_request_t;

// Sent from the file server to clients
#define FILE_SERVER_READ_FILE__PARTIAL_RESPONSE 104
typedef struct file_server_read_file_partial_response {
    uint32_t event;
    char path[64];
    uintptr_t data_length;
    uint8_t file_data[];
} file_server_read_file_partial_response_t;

#endif
