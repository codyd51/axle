#ifndef FILE_MANAGER_MESSAGES_H
#define FILE_MANAGER_MESSAGES_H

#include <kernel/amc.h>

#define FILE_MANAGER_SERVICE_NAME "com.axle.file_manager"
#define FILE_MANAGER_MAX_PATH_LENGTH 128

/*
 * awm signaling
 */

// Sent from file manager to awm to signal that it's ready to receive file requests
// Prior to this, awm should not request image resources, or else the request may 
// conflict with the file manager's attempts to create a window.
// TODO(PT): Think about ways to prevent this. Perhaps amc messages can be popped with a predicate
#define FILE_MANAGER_READY  100

/*
 * Create File/Directory
 */

// Sent from clients to the file manager
#define FILE_MANAGER_CREATE_FILE 101
typedef struct file_manager_create_file_request {
    uint32_t event; // Expects FILE_MANAGER_CREATE_FILE 
    char path[FILE_MANAGER_MAX_PATH_LENGTH];
} file_manager_create_file_request_t;

// Sent from the file manager to clients
#define FILE_MANAGER_CREATE_FILE_RESPONSE 101
typedef struct file_manager_create_file_response {
    uint32_t event; // Expects FILE_MANAGER_CREATE_FILE_RESPONSE
    bool success;
} file_manager_create_file_response_t;

// Sent from clients to the file manager
#define FILE_MANAGER_CREATE_DIRECTORY 102
typedef struct file_manager_create_directory_request {
    uint32_t event; // Expects FILE_MANAGER_CREATE_DIRECTORY 
    char path[FILE_MANAGER_MAX_PATH_LENGTH];
} file_manager_create_directory_request_t;

// Sent from the file manager to clients
#define FILE_MANAGER_CREATE_DIRECTORY_RESPONSE 102
typedef struct file_manager_create_directory_response {
    uint32_t event; // Expects FILE_MANAGER_CREATE_DIRECTORY_RESPONSE
    bool success;
} file_manager_create_directory_response_t;

/*
 * Delete File/Directory
 */

// Sent from clients to the file manager
#define FILE_MANAGER_DELETE_FILE 103
typedef struct file_manager_delete_file_request {
    uint32_t event; // Expects FILE_MANAGER_DELETE_FILE 
    char path[FILE_MANAGER_MAX_PATH_LENGTH];
} file_manager_delete_file_request_t;

// Sent from the file manager to clients
#define FILE_MANAGER_DELETE_FILE_RESPONSE 103
typedef struct file_manager_delete_file_response {
    uint32_t event; // Expects FILE_MANAGER_DELETE_FILE_RESPONSE
    bool success;
} file_manager_delete_file_response_t;

// Sent from clients to the file manager
#define FILE_MANAGER_DELETE_DIRECTORY 104
typedef struct file_manager_delete_directory_request {
    uint32_t event; // Expects FILE_MANAGER_DELETE_DIRECTORY 
    char path[FILE_MANAGER_MAX_PATH_LENGTH];
} file_manager_delete_directory_request_t;

// Sent from the file manager to clients
#define FILE_MANAGER_DELETE_DIRECTORY_RESPONSE 104
typedef struct file_manager_delete_directory_response {
    uint32_t event; // Expects FILE_MANAGER_DELETE_DIRECTORY_RESPONSE
    bool success;
} file_manager_delete_directory_response_t;

/*
 * Read a file
 */

// Sent from clients to the file manager
#define FILE_MANAGER_READ_FILE 105
typedef struct file_manager_read_file_request {
    uint32_t event; // Expects FILE_MANAGER_READ_FILE 
    char path[FILE_MANAGER_MAX_PATH_LENGTH];
} file_manager_read_file_request_t;

// Sent from the file manager to clients
#define FILE_MANAGER_READ_FILE_RESPONSE 105
typedef struct file_manager_read_file_response {
    uint32_t event; // Expects FILE_MANAGER_READ_FILE_RESPONSE
    uint32_t file_size;
    uint8_t file_data[];
} file_manager_read_file_response_t;

// Sent from clients to the file manager
#define FILE_MANAGER_READ_FILE__PARTIAL 106
typedef struct file_manager_read_file_partial_request {
    uint32_t event; // Expects FILE_MANAGER_READ_FILE 
    uint32_t offset;
    uint32_t length;
    char path[FILE_MANAGER_MAX_PATH_LENGTH];
} file_manager_read_file_partial_request_t;

// Sent from the file manager to clients
#define FILE_MANAGER_READ_FILE__PARTIAL_RESPONSE 106
typedef struct file_manager_read_file_partial_response {
    uint32_t event; // Expects FILE_MANAGER_READ_FILE__PARTIAL_RESPONSE
    uint32_t data_length;
    uint8_t file_data[];
} file_manager_read_file_partial_response_t;

/*
 * Write a file
 */

// Sent from the file manager to clients
#define FILE_MANAGER_WRITE_FILE 107
typedef struct file_manager_write_file {
    uint32_t event; // Expects FILE_MANAGER_WRITE_FILE
    uint32_t file_offset;
    uint32_t data_length;
    uint8_t file_data[];
} file_manager_write_file_t;

// Sent from the file manager to clients
#define FILE_MANAGER_WRITE_FILE_RESPONSE 107
typedef struct file_manager_write_file_response {
    uint32_t event; // Expects FILE_MANAGER_WRITE_FILE_RESPONSE
    uint32_t file_length;
} file_manager_write_file_response_t;

/*
 * Check if File Exists
 */

// Sent from clients to the file manager
#define FILE_MANAGER_CHECK_FILE_EXISTS 108
typedef struct file_manager_check_file_exists {
    uint32_t event; // Expects FILE_MANAGER_CHECK_FILE_EXISTS 
    char path[FILE_MANAGER_MAX_PATH_LENGTH];
} file_manager_check_file_exists_request_t;

// Sent from the file manager to clients
#define FILE_MANAGER_CHECK_FILE_EXISTS_RESPONSE 108
typedef struct file_manager_check_file_exists_response {
    uint32_t event; // Expects FILE_MANAGER_CHECK_FILE_EXISTS_RESPONSE
    char path[FILE_MANAGER_MAX_PATH_LENGTH];
    bool file_exists;
    uint32_t file_size;
} file_manager_check_file_exists_response_t;

/*
 * Launch a program by path
 */

// Sent from clients to the file manager
#define FILE_MANAGER_LAUNCH_FILE 109
typedef struct file_manager_launch_file_request {
    uint32_t event; // Expects FILE_MANAGER_LAUNCH_FILE
    char path[FILE_MANAGER_MAX_PATH_LENGTH];
} file_manager_launch_file_request_t;

#endif
