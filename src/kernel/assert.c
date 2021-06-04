#include <stdarg.h>
#include <stddef.h>

#include <std/printf.h>
#include <std/kheap.h>
#include <std/memory.h>

#include <kernel/boot_info.h>
#include <kernel/vmm/vmm.h>
#include <kernel/multitasking/tasks/task_small.h>

#include "assert.h"

#define _BACKTRACE_SIZE 16

void walk_stack(uint32_t out_stack_addrs[], int frame_count); 

void print_stack_trace(int frame_count) {
    printf("Stack trace:\n");
    uint32_t stack_addrs[_BACKTRACE_SIZE] = {0};
    walk_stack(stack_addrs, frame_count);
    for (int32_t i = 0; i < frame_count; i++) {
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
#include <kernel/util/amc/amc_internal.h>
// XXX(PT): Must match the definition in crash_reporter_messages.h
#define CRASH_REPORTER_SERVICE_NAME "com.axle.crash_reporter"
#define CRASH_REPORTER_INFORM_ASSERT 100
typedef struct crash_reporter_inform_assert {
    uint32_t event; // CRASH_REPORTER_INFORM_ASSERT
    uint32_t crash_report_length;
    char crash_report[];
} crash_reporter_inform_assert_t;

// XXX(PT): Must match the definition in file_manager_messages.h
#define FILE_MANAGER_SERVICE_NAME "com.axle.file_manager"
#define FILE_MANAGER_LAUNCH_FILE 103
typedef struct file_manager_launch_file_request {
    uint32_t event; // FILE_MANAGER_LAUNCH_FILE
    char path[128];
} file_manager_launch_file_request_t;

bool append(char** buf_head, int32_t* buf_size, const char* format, ...) {
    va_list args;
    va_start(args, format);

    char* orig_buf_head = *buf_head;

    *buf_head += vsnprintf(*buf_head, *buf_size, format, args);
    *buf_size -= ((*buf_head) - orig_buf_head);

    va_end(args);
    if (*buf_size <= 1) {
        return false;
    }
    return true;
}

bool symbolicate_and_append(int frame_idx, uint32_t* frame_addr, char** buf_head, int32_t* buf_size) {
    char symbol[128] = {0};

    bool found_program_start = false;

    // Is the frame mapped within the kernel address space?
    if (vmm_address_is_mapped(boot_info_get()->vmm_kernel, frame_addr)) {
        const char* kernel_symbol = elf_sym_lookup(&boot_info_get()->kernel_elf_symbol_table, (uint32_t)frame_addr);
        snprintf(symbol, sizeof(symbol), "[Kernel] %s", kernel_symbol ?: "-");
    }
    else {
        task_small_t* current_task = tasking_get_current_task();
        const char* program_symbol = elf_sym_lookup(&current_task->elf_symbol_table, (uint32_t)frame_addr);
        snprintf(symbol, sizeof(symbol), "[%s] %s", current_task->name, program_symbol);
        if (!strncmp(program_symbol, "_start", 8)) {
            found_program_start = true;
        }
    }

    bool can_append_more = append(buf_head, buf_size, "[%02d] 0x%08x %s\n", frame_idx, (uint32_t)frame_addr, symbol);
    if (!can_append_more || found_program_start) {
        return false;
    }
    return true;
}

void task_build_and_send_crash_report_then_exit(const char* msg, const register_state_t* regs) {
    // Launch the crash reporter if it's not active
    if (!amc_service_is_active(CRASH_REPORTER_SERVICE_NAME)) {
        file_manager_launch_file_request_t req = {0};
        req.event = FILE_MANAGER_LAUNCH_FILE;
        snprintf(req.path, sizeof(req.path), "crash_reporter");
        amc_message_construct_and_send(FILE_MANAGER_SERVICE_NAME, &req, sizeof(file_manager_launch_file_request_t));
    }

    int32_t buf_size = 1024;
    char* crash_report_buf = kmalloc(buf_size);
    char* crash_report_ptr = crash_report_buf;

    // The unrolled loop is necessary as __builtin_return_address requires a literal argument
    // The goto is necessary to avoid deeply nested failure handling
    if (!append(&crash_report_ptr, &buf_size, "Cause of death:\n%s\n", msg)) goto finish_fmt;

    if (regs != NULL) {
        task_small_t* current_task = tasking_get_current_task();
        if (!append(&crash_report_ptr, &buf_size, "\nRegisters:\n")) goto finish_fmt;
        if (!append(&crash_report_ptr, &buf_size, "eip: 0x%08x  useresp 0x%08x\n", regs->eip, regs->useresp)) goto finish_fmt;
        if (!append(&crash_report_ptr, &buf_size, "eax: 0x%08x  ebx 0x%08x  ecx 0x%08x  edx 0x%08x\n", regs->eax, regs->ebx, regs->ecx, regs->edx)) goto finish_fmt;
        if (!append(&crash_report_ptr, &buf_size, "edi: 0x%08x  esi 0x%08x  ebp 0x%08x  esp 0x%08x\n", regs->edi, regs->esi, regs->ebp, regs->esp)) goto finish_fmt;
    }

    if (!append(&crash_report_ptr, &buf_size, "\nStack trace:\n")) goto finish_fmt;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-address" 
    if (!symbolicate_and_append(0, __builtin_return_address(0), &crash_report_ptr, &buf_size)) goto finish_fmt;
    if (!symbolicate_and_append(1, __builtin_return_address(1), &crash_report_ptr, &buf_size)) goto finish_fmt;
    if (!symbolicate_and_append(2, __builtin_return_address(2), &crash_report_ptr, &buf_size)) goto finish_fmt;
    if (!symbolicate_and_append(3, __builtin_return_address(3), &crash_report_ptr, &buf_size)) goto finish_fmt;
    if (!symbolicate_and_append(4, __builtin_return_address(4), &crash_report_ptr, &buf_size)) goto finish_fmt;
    if (!symbolicate_and_append(5, __builtin_return_address(5), &crash_report_ptr, &buf_size)) goto finish_fmt;
    if (!symbolicate_and_append(6, __builtin_return_address(6), &crash_report_ptr, &buf_size)) goto finish_fmt;
    if (!symbolicate_and_append(7, __builtin_return_address(7), &crash_report_ptr, &buf_size)) goto finish_fmt;
    if (!symbolicate_and_append(8, __builtin_return_address(8), &crash_report_ptr, &buf_size)) goto finish_fmt;
    if (!symbolicate_and_append(9, __builtin_return_address(9), &crash_report_ptr, &buf_size)) goto finish_fmt;
    if (!symbolicate_and_append(10, __builtin_return_address(10), &crash_report_ptr, &buf_size)) goto finish_fmt;
    if (!symbolicate_and_append(11, __builtin_return_address(11), &crash_report_ptr, &buf_size)) goto finish_fmt;
    if (!symbolicate_and_append(12, __builtin_return_address(12), &crash_report_ptr, &buf_size)) goto finish_fmt;
    if (!symbolicate_and_append(13, __builtin_return_address(13), &crash_report_ptr, &buf_size)) goto finish_fmt;
    if (!symbolicate_and_append(14, __builtin_return_address(14), &crash_report_ptr, &buf_size)) goto finish_fmt;
    if (!symbolicate_and_append(15, __builtin_return_address(15), &crash_report_ptr, &buf_size)) goto finish_fmt;
#pragma GCC diagnostic pop

finish_fmt:
    // No-op because we can't have a declaration directly after a label
    do {} while (0);

    uint32_t crash_report_len = crash_report_ptr - crash_report_buf;
    uint32_t crash_report_msg_len = sizeof(crash_reporter_inform_assert_t) + crash_report_len;
    crash_reporter_inform_assert_t* inform = kmalloc(crash_report_msg_len);
    inform->event = CRASH_REPORTER_INFORM_ASSERT;
    inform->crash_report_length = crash_report_len;
    memcpy(&inform->crash_report, crash_report_buf, crash_report_len);
    kfree(crash_report_buf);
    amc_message_construct_and_send(CRASH_REPORTER_SERVICE_NAME, inform, crash_report_msg_len);
    kfree(inform);

    exit(1);
}

static bool _can_send_crash_report(void) {
    return amc_is_active() && amc_service_is_active(FILE_MANAGER_SERVICE_NAME);
}

void task_assert(bool cond, const char* msg, const register_state_t* regs) {
    if (cond) {
        return;
    }

    if (!_can_send_crash_report()) {
        assert(cond, msg);
    }
    else {
        task_build_and_send_crash_report_then_exit(msg, regs);
    }
}
