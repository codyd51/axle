#include <stdarg.h>
#include <stddef.h>

#include <std/printf.h>
#include <std/kheap.h>
#include <std/memory.h>
#include <gfx/lib/gfx.h>

#include <kernel/kernel.h>
#include <kernel/boot_info.h>
#include <kernel/vmm/vmm.h>
#include <kernel/multitasking/tasks/task_small.h>
#include <kernel/syscall/sysfuncs.h>
#include <kernel/drivers/pit/pit.h>

#include <kernel/util/amc/amc.h>
#include <kernel/util/amc/amc_internal.h>
#include <file_server/file_server_messages.h>
#include <crash_reporter/crash_reporter_messages.h>

#include "assert.h"

#define _BACKTRACE_SIZE 16

void walk_stack(uintptr_t out_stack_addrs[], int frame_count); 
bool symbolicate_and_append(int frame_idx, uintptr_t* frame_addr, char** buf_head, int32_t* buf_size);
uint64_t _get_return_address_of_stack_frame(int frame_idx, const register_state_t* regs);

void print_stack_trace(int frame_count) {
    printf("Stack trace (want %d frames):\n", frame_count);
    uintptr_t stack_addrs[_BACKTRACE_SIZE] = {0};
    walk_stack(stack_addrs, frame_count);
    for (int32_t i = 0; i < frame_count; i++) {
        uintptr_t frame_addr = stack_addrs[i];
        if (!frame_addr) {
            printf("No frame address for frame %d\n", i);
            //break;
            continue;
        }
        printf("[%d] 0x%p ", i, frame_addr);
        if (frame_addr >= VAS_KERNEL_CODE_BASE) {
            const char* kernel_symbol = elf_sym_lookup(&boot_info_get()->kernel_elf_symbol_table, (uintptr_t)frame_addr);
            printf("[Kernel] %s", kernel_symbol ?: "-");
        }
        printf("\n");
    }
}

void panic_with_regs(const char* msg, register_state_x86_64_t* regs) {
    asm("cli");

    int buf_size = 2048;
    char buf[buf_size];

    // Error message
    snprintf(buf, sizeof(buf), "Critical error! %s\n", msg);
    kernel_gfx_write_line_rendered_string(buf);

    // RIP
    snprintf(buf, sizeof(buf), "RIP: 0x%x", regs->return_rip);
    kernel_gfx_write_line_rendered_string(buf);

    // Stack trace
    char* buf_ptr = buf;
    memset(buf_ptr, 0, buf_size);

    for (int i = 0; i < 16; i++) {
        if (!symbolicate_and_append(i, _get_return_address_of_stack_frame(i, regs), &buf_ptr, &buf_size)) {
            break;
        }
    }
    kernel_gfx_write_line_rendered_string("Stack trace:\n");
    kernel_gfx_write_line_rendered_string(buf);

    asm("hlt");
}

void _panic(const char* msg, const char* file, int line) {
    //enter infinite loop
    asm("cli");
    printf("[%d] Assertion failed: %s\n", getpid(), msg);
    printf("%s:%d\n", file, line);
    panic_with_regs(msg, NULL);
}

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

bool symbolicate_and_append__user_mode_frame(int frame_idx, uintptr_t* frame_addr, char** buf_head, int32_t* buf_size) {
    return symbolicate_and_append(frame_idx, frame_addr, buf_head, buf_size);
}

bool symbolicate_and_append(int frame_idx, uintptr_t* frame_addr, char** buf_head, int32_t* buf_size) {
    printf("symbolicate(%d, 0x%x) = ", frame_idx, frame_addr);

    if (frame_addr < PAGE_SIZE) {
        // If we've traversed to the NULL page, we're probably out of stack frames
        printf("NULL\n");
        return false;
    }

    char symbol[128] = {0};
    bool found_program_start = false;

    // Is the frame mapped within the kernel address space?
    if (frame_addr >= VAS_KERNEL_CODE_BASE) {
        const char* kernel_symbol = elf_sym_lookup(&boot_info_get()->kernel_elf_symbol_table, (uintptr_t)frame_addr);
        printf("kernel symbol %s\n", kernel_symbol ?: "-");
        snprintf(symbol, sizeof(symbol), "[Kernel] %s", kernel_symbol ?: "-");
        
        // Note: Root function of exec() in core amc commands
        // Note: Root function of fs_server launch in the kernel
        if (kernel_symbol != NULL && (!strncmp(kernel_symbol, AMC_EXEC_TRAMPOLINE_NAME_STR, 32) || !strncmp(kernel_symbol, FS_SERVER_EXEC_TRAMPOLINE_NAME_STR, 32))) {
            found_program_start = true;
        }
    }
    else {
        task_small_t* current_task = tasking_get_current_task();
        const char* program_symbol = elf_sym_lookup(&current_task->elf_symbol_table, (uintptr_t)frame_addr);
        snprintf(symbol, sizeof(symbol), "[%s] %s", current_task->name, program_symbol);
        if (!strncmp(program_symbol, "_start", 7)) {
            found_program_start = true;
        }
    }

    printf("[%02d] 0x%p %s\n", frame_idx, (uintptr_t)frame_addr, symbol);
    bool can_append_more = append(buf_head, buf_size, "[%02d] 0x%p %s\n", frame_idx, (uintptr_t)frame_addr, symbol);
    if (!can_append_more || found_program_start) {
        return false;
    }
    return true;
}

uint64_t _get_return_address_of_stack_frame(int frame_idx, const register_state_t* regs) {
    //printf("__builting_return_address2 %d 0x%x\n", frame_idx, regs);

    if (!regs) {
        printf("No regs passed\n");
        return 0;
    }

    uint64_t* rbp = regs->rbp;
    if (rbp < USER_MODE_STACK_BOTTOM) {
        //printf("RBP too small\n");
        return 0;
    }

    // TODO(PT): The above check can be entirely replaced by this one?
    if (rbp < VAS_KERNEL_HEAP_BASE && !vas_is_page_present(vas_get_active_state(), rbp)) {
        printf("RBP 0x%x is unmapped!\n", rbp);
        return 0;
    }

    //printf("\t RBP 0x%x\n", rbp);
    uint64_t rip = rbp[1];
    //printf("\t RIP 0x%x\n", rip);
    for (int j = 0; j < frame_idx; j++) {
        if (rbp < USER_MODE_STACK_BOTTOM) {
            return 0;
        }

        rbp = rbp[0];

        if (rbp < USER_MODE_STACK_BOTTOM) {
            return 0;
        }
        //printf("\t Next, RBP 0x%x\n", rbp);
        rip = rbp[1];
        //printf("\t RIP 0x%x\n", rip);
    }
    return rip;
}

void task_build_and_send_crash_report_then_exit(const char* msg, const register_state_t* regs) {
    // Launch the crash reporter if it's not active
    printf("task_build_and_send_crash_report_then_exit(%s, 0x%x)\n", msg, regs);
    if (!amc_service_is_active(CRASH_REPORTER_SERVICE_NAME)) {
        printf("Launching crash reporter...\n");

        file_server_launch_program_t req = {0};
        req.event = FILE_SERVER_LAUNCH_PROGRAM;
        snprintf(req.path, sizeof(req.path), "/usr/applications/crash_reporter");

        amc_message_send(FILE_SERVER_SERVICE_NAME, &req, sizeof(file_server_launch_program_t));
        printf("Sent message to file server!\n");
    }

    int32_t buf_size = 2048;
    char* crash_report_buf = kmalloc(buf_size);
    char* crash_report_ptr = crash_report_buf;

    // The unrolled loop is necessary as __builtin_return_address requires a literal argument
    // The goto is necessary to avoid deeply nested failure handling
    printf("Appending...\n");
    if (!append(&crash_report_ptr, &buf_size, "Cause of death:\n%s\n", msg)) goto finish_fmt;

    if (regs != NULL) {
        printf("Appending registers\n");
        task_small_t* current_task = tasking_get_current_task();

#if defined __i386__
        if (!append(&crash_report_ptr, &buf_size, "\nRegisters:\n")) goto finish_fmt;
        if (!append(&crash_report_ptr, &buf_size, "eip: 0x%08x  useresp 0x%08x\n", regs->eip, regs->useresp)) goto finish_fmt;
        if (!append(&crash_report_ptr, &buf_size, "eax: 0x%08x  ebx 0x%08x  ecx 0x%08x  edx 0x%08x\n", regs->eax, regs->ebx, regs->ecx, regs->edx)) goto finish_fmt;
        if (!append(&crash_report_ptr, &buf_size, "edi: 0x%08x  esi 0x%08x  ebp 0x%08x  esp 0x%08x\n", regs->edi, regs->esi, regs->ebp, regs->esp)) goto finish_fmt;
#elif defined __x86_64__
        printf("Appending registers ...\n");
        if (!append(&crash_report_ptr, &buf_size, "\nRegisters:\n")) goto finish_fmt;
        if (!append(&crash_report_ptr, &buf_size, "rip 0x%p  rsp 0x%p\n", regs->return_rip, regs->return_rsp)) goto finish_fmt;
        if (!append(&crash_report_ptr, &buf_size, "rax 0x%p  rbx 0x%p  rcx 0x%p rdx 0x%p\n", regs->rax, regs->rbx, regs->rcx, regs->rdx)) goto finish_fmt;
        if (!append(&crash_report_ptr, &buf_size, "rdi 0x%p  rsi 0x%p  rbp 0x%p\n", regs->rdi, regs->rsi, regs->rbp)) goto finish_fmt;
        if (!append(&crash_report_ptr, &buf_size, "r8  0x%p  r9  0x%p  r10 0x%p r11 0x%p\n", regs->r8, regs->r9, regs->r10, regs->r11)) goto finish_fmt;
        if (!append(&crash_report_ptr, &buf_size, "r12 0x%p  r13 0x%p  r14 0x%p r15 0x%p\n", regs->r12, regs->r13, regs->r14, regs->r15)) goto finish_fmt;
        printf("Finished appending registers...\n");
#else 
        FAIL_TO_COMPILE();
#endif
    }

    if (regs) {
        if (!append(&crash_report_ptr, &buf_size, "\nStack trace:\n")) goto finish_fmt;
        for (int i = 0; i < 16; i++) {
            if (!symbolicate_and_append(i, _get_return_address_of_stack_frame(i, regs), &crash_report_ptr, &buf_size)) {
                break;
            }
        }
    }

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
    amc_message_send(CRASH_REPORTER_SERVICE_NAME, inform, crash_report_msg_len);
    kfree(inform);

    exit(1);
}

bool should_relaunch_crashed_amc_service(amc_service_t* service) {
    return false;
    if (!service) {
        return false;
    }

    if (!strncmp(service->name, FILE_SERVER_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
        return true;
    }

    return false;
}

static bool _can_send_crash_report(void) {
    printf("can_send_crash_report?\n");
    /*
    if (!amc_is_active() || !amc_service_is_active(FILE_SERVER_SERVICE_NAME)) {
        printf("Cannot generate crash report because the file server service wasn't active\n");
        return false;
    }
    */
    if (!amc_is_active()) {
        return false;
    }

    // We can only send a crash report if the process that died isn't 
    // - com.axle.awm
    // - com.axle.file_server
    // - com.axle.crash_reporter
    // All three are necessary for user-visible crash reports
    amc_service_t* s = amc_service_of_active_task();
    if (!s) {
        // Register an AMC service so we can send a crash report
        char buf[AMC_MAX_SERVICE_NAME_LEN];
        snprintf(buf, sizeof(buf), "com.axle.corpse_service_PID_%d_time_%d", getpid(), ms_since_boot());
        amc_register_service(buf);
        s = amc_service_of_active_task();
        assert(s != NULL, "Expected an amc service to be registered");
    }

    if (!strncmp(s->name, FILE_SERVER_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN) ||
    //if (!strncmp(s->name, CRASH_REPORTER_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN) ||
        !strncmp(s->name, CRASH_REPORTER_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN) ||
        !strncmp(s->name, "com.axle.awm", AMC_MAX_SERVICE_NAME_LEN)) {
        printf("Cannot generate crash report because the died process is critical to crash-reporting: %s\n", s->name);
        return false;
    }

    return true;
}

#include <gfx/lib/gfx.h>

void task_assert(bool cond, const char* msg, const register_state_t* regs) {
    if (cond) {
        return;
    }

    printf("task_assert %s 0x%x\n", msg, regs);

    // Immediately disable amc delivery
    // If the file server has just crashed, we might send a message to launch the 
    // crash reporter. We don't want the message to be delivered to the crashed instance of the
    // file server, and instead want the message to hang around to be picked up by the new instance.
    amc_disable_delivery(amc_service_of_active_task());

    //assert(cond, msg);
    if (!_can_send_crash_report()) {
        char buf[512];
        amc_service_t* amc_service = amc_service_of_active_task();
        char* amc_service_name = amc_service ? amc_service->name : "-";
        snprintf(buf, 512, "1/3 key programs died: %s", amc_service_name);
        /*
        draw_string_oneshot(buf);
        */
        Size screen_size = kernel_gfx_screen_size();
        int box_of_death_height = screen_size.height / 3;
        Rect box_of_death = rect_make(
            point_make(0, (screen_size.height / 2) - (box_of_death_height / 2)),
            size_make(screen_size.width, box_of_death_height)
        );
        kernel_gfx_fill_rect(box_of_death, color_make(204, 31, 65));
        kernel_gfx_set_line_rendered_string_cursor(point_make(10, rect_min_y(box_of_death) + 10));
        kernel_gfx_write_line_rendered_string(buf);
        panic_with_regs(msg, regs);
        //exit(1);
    }
    else {
        task_build_and_send_crash_report_then_exit(msg, regs);
    }
}
