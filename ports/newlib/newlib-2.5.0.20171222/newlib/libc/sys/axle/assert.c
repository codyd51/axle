#include <stdio.h>
#include <stdlib.h>

#include "assert.h"
#include <kernel/amc.h>

// XXX(PT): Must match the definition in crash_reporter_messages.h
#define CRASH_REPORTER_SERVICE_NAME "com.axle.crash_reporter"
#define CRASH_REPORTER_INFORM_ASSERT 100
typedef struct crash_reporter_inform_assert {
    uint32_t event; // CRASH_REPORTER_INFORM_ASSERT
    char assert_message[128];
} crash_reporter_inform_assert_t;

// XXX(PT): Must match the definition in file_manager_messages.h
#define FILE_MANAGER_SERVICE_NAME "com.axle.file_manager"
#define FILE_MANAGER_LAUNCH_FILE 103
typedef struct file_manager_launch_file_request {
    uint32_t event; // FILE_MANAGER_LAUNCH_FILE
    char path[128];
} file_manager_launch_file_request_t;

void assert(bool cond, const char* msg) {
	if (!cond) {
		printf("Assertion failed: %s\n", msg);

		// Launch the crash reporter if it's not active
		if (!amc_service_is_active(CRASH_REPORTER_SERVICE_NAME)) {
			printf("Requesting launch of crash reporter\n");
			file_manager_launch_file_request_t req = {0};
			req.event = FILE_MANAGER_LAUNCH_FILE;
			snprintf(req.path, sizeof(req.path), "crash_reporter");
			amc_message_construct_and_send(FILE_MANAGER_SERVICE_NAME, &req, sizeof(file_manager_launch_file_request_t));
		}

		crash_reporter_inform_assert_t inform = {0};
		inform.event = CRASH_REPORTER_INFORM_ASSERT;
		snprintf(inform.assert_message, sizeof(inform.assert_message), "Assertion failed: %s", msg);
		amc_message_construct_and_send(CRASH_REPORTER_SERVICE_NAME, &inform, sizeof(inform));

		exit(1);
	}
}
