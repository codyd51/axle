#include <stddef.h>
#include <stdint.h>

#include <kernel/boot_info.h>

// Arch-specific
#include <kernel/segmentation/gdt.h>

// Memory
#include <kernel/pmm/pmm.h>
#include <kernel/vmm/vmm.h>

// Peripherals interaction
#include <kernel/drivers/kb/kb.h>
#include <kernel/drivers/pit/pit.h>
#include <kernel/drivers/mouse/mouse.h>
#include <kernel/drivers/serial/serial.h>

// Kernel features
#include <kernel/syscall/syscall.h>
#include <kernel/util/elf/elf.h>
#include <kernel/multitasking/tasks/task_small.h>

#include "kernel.h"

static void _kernel_bootstrap_part2(void);

void FS_SERVER_EXEC_TRAMPOLINE_NAME(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    boot_info_t* boot_info = boot_info_get();
    const char* program_name = "fs_server";
    char* argv[] = {program_name, NULL};
    elf_load_buffer(program_name, argv, PMA_TO_VMA(boot_info->file_server_elf_start), boot_info->file_server_elf_size, false);
}

void _start(axle_boot_info_t* boot_info) {
    // Environment info
    boot_info_read(boot_info);
    boot_info_dump();

    // Descriptor tables
    gdt_init();
    interrupt_init();

    // PIT and serial drivers
    pit_timer_init(PIT_TICK_GRANULARITY_1MS);
    serial_init();

    // Kernel features
    pmm_init();
    pmm_dump();
    vmm_init(boot_info->boot_pml4);

    // Higher-level features like multitasking
    tasking_init(&_kernel_bootstrap_part2);
    // The above call should never return
    assert(false, "Control should have been transferred to a new stack");
}

static void _kernel_bootstrap_part2(void) {
    // We're now fully set up in high memory
    syscall_init();

    // Initialize PS/2 controller
    // (and sub-drivers, such as a PS/2 keyboard and mouse)
    ps2_controller_init();

    // Early boot is finished
    // Multitasking and program loading is now available
    // 
    // Launch the file server, which will load the ramdisk and launch all the 
    // specified startup programs
    task_spawn__with_args("launch_fs_server", FS_SERVER_EXEC_TRAMPOLINE_NAME, 0, 0, 0);

    // Bootstrapping complete - kill this process
    printf("[t = %d] Bootstrap task [PID %d] will exit\n", time(), getpid());
    task_die(0);
    assert(0, "task_die should have stopped execution");
}
