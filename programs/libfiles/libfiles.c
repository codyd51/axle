#include <errno.h>
#include <stdio.h>
#include <sys/fcntl.h>

#include <file_manager/file_manager_messages.h>
#include <stdlibadd/assert.h>
#include <stdlibadd/array.h>

#include <libamc/libamc.h>

#include "libfiles.h"

typedef enum file_mode {
    FILE_MODE_READ_ONLY = 0,
    FILE_MODE_WRITE_ONLY = 1,
    FILE_MODE_READ_WRITE = 2
} file_mode_t;

typedef struct file_entry {
    bool allocated;
    file_mode_t mode;
    char path[FILE_MANAGER_MAX_PATH_LENGTH];
    uint32_t file_size;
    int offset;
} file_entry_t;

#define MAX_FILES 128
file_entry_t _g_file_entries[MAX_FILES] = {0};
bool _g_initialised = false;

static void files_init(void) {
    _g_initialised = true;

    // Reserve the standard streams
    _g_file_entries[0].allocated = true;
    _g_file_entries[1].allocated = true;
    _g_file_entries[2].allocated = true;
}

static file_entry_t* file_entry_alloc(int* out_descriptor) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (_g_file_entries[i].allocated == false) {
            file_entry_t* f = &_g_file_entries[i];
            f->allocated = true;
            *out_descriptor = i;
            return f;
        }
    }
    assert(false, "Ran out of file descriptors!");
    return -1;
}

amc_message_t* file_manager_get_response(uint32_t expected_event) {
	amc_message_t* response_msg = NULL;
    bool received_response = false;
	for (uint32_t i = 0; i < 32; i++) {
		amc_message_await(FILE_MANAGER_SERVICE_NAME, &response_msg);
		uint32_t event = amc_msg_u32_get_word(response_msg, 0);
		if (event == expected_event) {
			received_response = true;
			break;
		}
	}
    assert(received_response, "Failed to receive response from file_manager");
    return response_msg;
}

int mkdir(const char* pathname, mode_t mode) {
    printf("mkdir(%s, %ld)\n", pathname, mode);
    file_manager_create_directory_request_t req = {0};
    req.event = FILE_MANAGER_CREATE_DIRECTORY;
    snprintf(req.path, sizeof(req.path), "%s", pathname);
    amc_message_construct_and_send(FILE_MANAGER_SERVICE_NAME, &req, sizeof(req));

    amc_message_t* response_msg = file_manager_get_response(FILE_MANAGER_CREATE_DIRECTORY_RESPONSE);
    file_manager_create_directory_response_t* resp = (file_manager_create_directory_response_t*)&response_msg->body;
    if (resp->success) {
        return 0;
    }
    // TODO(PT): Set errno?
    return -1;
}

int open(const char* name, int flags, ...) {
    printf("open(%s, %ld)\n", name, flags);
    printf("Value of O_RDONLY %ld O_WRONLY %ld O_RDWR %ld\n", O_RDONLY, O_WRONLY, O_RDWR);

    if (!_g_initialised) {
        files_init();
    }

    /*
    if (flags & O_CREAT) {
        assert(false, "create not implemented");
        return -1;
    }
    */
    if (flags & O_EXCL) {
        assert(false, "O_EXCL not implemented");
        return -1;
    }

    // O_RDONLY is defined as 0, so we can't check if a bit is set
    file_mode_t mode = FILE_MODE_READ_ONLY;
    if (flags & O_WRONLY) {
        mode = FILE_MODE_WRITE_ONLY;
    }
    else if (flags & O_RDWR) {
        mode = FILE_MODE_READ_WRITE;
    }

    // Does the file exist?
    file_manager_check_file_exists_request_t req = {0};
    req.event = FILE_MANAGER_CHECK_FILE_EXISTS;
    snprintf(req.path, sizeof(req.path), "%s", name);
    amc_message_construct_and_send(FILE_MANAGER_SERVICE_NAME, &req, sizeof(req));

    amc_message_t* response_msg = file_manager_get_response(FILE_MANAGER_CHECK_FILE_EXISTS_RESPONSE);
    file_manager_check_file_exists_response_t* resp = (file_manager_check_file_exists_response_t*)&response_msg->body;
    assert(!strncmp(req.path, resp->path, FILE_MANAGER_MAX_PATH_LENGTH), "File manager responded about a different file?");
    if (!resp->file_exists) {
        errno = ENOENT;
        return -1;
    }

    int out_descriptor = -1;
    file_entry_t* entry = file_entry_alloc(&out_descriptor);
    entry->mode = mode;
    snprintf(entry->path, sizeof(entry->path), "%s", name);

    printf("open(%s, %ld): Successfully opened file! fd = %ld\n", name, flags, out_descriptor);

    entry->file_size = resp->file_size;

    return out_descriptor;
}

int close(int fildes) {
    printf("close(%ld)\n", fildes);
    if (fildes < 0 || fildes >= MAX_FILES) {
        errno = EBADF;
        return -1;
    }

    file_entry_t* entry = &_g_file_entries[fildes];
    if (!entry->allocated) {
        errno = EBADF;
        return -1;
    }

    printf("close(%ld) closing %s, opened in mode %ld\n", fildes, entry->path, entry->mode);
    entry->allocated = false;

    return 0;
}

int lseek(int fildes, off_t offset, int whence) {
    //printf("lseek(%ld, %ld, %ld) = ", fildes, offset, whence);
    if (offset == 3830948 || offset == 3492172) {
        print_memory();
    }

    file_entry_t* entry = &_g_file_entries[fildes];
    if (!entry->allocated) {
        errno = EBADF;
        return -1;
    }

    int32_t new_offset = 0;
    if (whence == SEEK_SET) {
        new_offset = offset;
    }
    else if (whence == SEEK_CUR) {
        new_offset += offset;
    }
    else if (whence == SEEK_END) {
        new_offset = entry->file_size + offset;
    }
    else {
        assert(false, "Unimplementend 'whence' type");
        return -1;
    }

    if (new_offset < 0) {
        errno = EINVAL;
        return -1;
    }

    entry->offset = new_offset;
    //printf("%ld\n", entry->offset);
    return entry->offset;
}

ssize_t read(int fildes, void* buf, size_t nbyte) {
    //printf("read(%ld, 0x%08lx, %ld)\n", fildes, buf, nbyte);

    file_entry_t* entry = &_g_file_entries[fildes];
    if (!entry->allocated) {
        errno = EBADF;
        return -1;
    }

    file_manager_read_file_partial_request_t req = {0};
    req.event = FILE_MANAGER_READ_FILE__PARTIAL;
    req.length = nbyte;
    req.offset = entry->offset;
    snprintf(req.path, sizeof(req.path), "%s", entry->path);
    amc_message_construct_and_send(FILE_MANAGER_SERVICE_NAME, &req, sizeof(req));

    amc_message_t* response_msg = file_manager_get_response(FILE_MANAGER_READ_FILE__PARTIAL_RESPONSE);
    file_manager_read_file_partial_response_t* resp = (file_manager_read_file_partial_response_t*)&response_msg->body;

    //printf("Read got response! Length: %ld\n", resp->data_length);
    entry->offset += resp->data_length;
    memcpy(buf, resp->file_data, resp->data_length);
    return resp->data_length;
}
