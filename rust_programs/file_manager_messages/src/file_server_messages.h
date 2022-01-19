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

#endif
