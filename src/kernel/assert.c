#include "assert.h"
#include <kernel/boot_info.h>

#define _BACKTRACE_SIZE 16

void print_stack_trace(int frame_count) {
    printf("Stack trace:\n");
    uint32_t stack_addrs[_BACKTRACE_SIZE] = {0};
    walk_stack(stack_addrs, frame_count);
    for (uint32_t i = 0; i < frame_count; i++) {
        int frame_addr = stack_addrs[i];
        if (!frame_addr) {
            break;
        }
        printf("[%d] 0x%08x\n", i, frame_addr);
    }
}

void _panic(const char* msg, const char* file, int line) {
    //enter infinite loop
    asm("cli");
    printf("[%d] Assertion failed: %s\n", getpid(), msg);
    printf("%s:%d\n", file, line);
    if (true) {
        print_stack_trace(20);
    }
    asm("cli");
    asm("hlt");
}

#include <kernel/util/amc/amc.h>
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

void task_build_and_send_crash_report_then_exit(const char* msg) {
    // Launch the crash reporter if it's not active
    if (!amc_service_is_active(CRASH_REPORTER_SERVICE_NAME)) {
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

static bool _can_send_crash_report(void) {
    return amc_is_active() && amc_service_is_active(FILE_MANAGER_SERVICE_NAME);
}

void task_assert(bool cond, const char* msg) {
    if (cond) {
        return;
    }

    if (!_can_send_crash_report()) {
        assert(cond, msg);
    }
    else {
        task_build_and_send_crash_report_then_exit(msg);
    }
}
