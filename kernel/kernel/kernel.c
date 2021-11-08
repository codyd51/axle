// Freestanding headers
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Kernel stdlib headers
#include <std/printf.h>
#include <std/string.h>

// Kernel headers
#include <kernel/multiboot.h>
#include <kernel/boot.h>
#include <kernel/elf.h>
#include <kernel/assert.h>
#include <kernel/boot_info.h>
#include <kernel/segmentation/gdt.h>
#include <kernel/interrupts/interrupts.h>

// Kernel drivers
#include <kernel/drivers/kb/kb.h>
#include <kernel/drivers/mouse/mouse.h>
#include <kernel/drivers/pit/pit.h>
#include <kernel/drivers/serial/serial.h>

// Higher-level kernel features
#include <std/kheap.h>
#include <kernel/pmm/pmm.h>
#include <kernel/vmm/vmm.h>
#include <kernel/syscall/syscall.h>
#include <kernel/multitasking/tasks/task_small.h>
#include <kernel/util/elf/elf.h>
#include <kernel/util/amc/amc.h>
#include <kernel/util/vfs/vfs.h>
#include <bootloader/axle_boot_info.h>

static void _kernel_bootstrap_part2(void);

static void _launch_program(const char* program_name, uint32_t arg2, uint32_t arg3) {
    printf("_launch_program(%s, 0x%08x 0x%08x)\n", program_name, arg2, arg3);
    char* argv[] = {program_name, NULL};

    initrd_fs_node_t* node = vfs_find_initrd_node_by_name(program_name);
    if (!node) {
        panic("Program specified in boot list wasn't found in initrd");
    }
    uintptr_t address = PMA_TO_VMA(node->initrd_offset);
	elf_load_buffer(program_name, address, node->size, argv);
	panic("noreturn");
}

uintptr_t initial_esp = 0;
//void kernel_main(struct multiboot_info* mboot_ptr, uint32_t initial_stack) {
int _start(axle_boot_info_t* boot_info) {
    //debug_paging(boot_info);
    //initial_esp = initial_stack;

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
    //debug_paging(boot_info);

    vmm_init(boot_info->boot_pml4);
    /*
    char* a = kmalloc(512);
    printf("a 0x%p\n", a);
    kfree(a);
    */

    //vmm_init();
    //vmm_notify_shared_kernel_memory_allocated();

    /*
    Low code, low data
    Data - high CR3
    Memcpy code and data to high address
    Load high cr3
    Jump to high address
    */

    // We've now allocated all the kernel memory that'll be mapped into every process 
    // Inform the VMM so that it can begin allocating memory outside of shared page tables
    //vmm_dump(boot_info_get()->vmm_kernel);

    // Higher-level features like multitasking
    tasking_init(&_kernel_bootstrap_part2);
    // The above call should never return
    assert(false, "Control should have been transferred to a new stack");
}

static void _nop() {
    printf("Task running\n");
    amc_register_service("com.axle.nop");
    task_die(0);
}

static void _kernel_bootstrap_part2(void) {
    // We're now fully set up in high memory

    syscall_init();
    vfs_init();

    // Initialize PS/2 controller
    // (and sub-drivers, such as a PS/2 keyboard and mouse)
    ps2_controller_init();

    // Early boot is finished
    // Multitasking and program loading is now available

    // Launch some initial drivers and services
    // TODO(PT): Launching a not-present program here causes a page fault in elf_validate_header. Handle it gracefully...
    const char* launch_programs[] = {
        // VFS / program launcher
        "file_manager",
        // HDD FS dependency
        //"ata_driver",
        // Window manager
        "awm",
        // User input
        "kb_driver",
        "mouse_driver",
    };
    for (uint32_t i = 0; i < sizeof(launch_programs) / sizeof(launch_programs[0]); i++) {
        const char* program_name = launch_programs[i];
        task_spawn__with_args(program_name, _launch_program, program_name, 0, 0);
    }

    // Bootstrapping complete - kill this process
    printf("[t = %d] Bootstrap task [PID %d] will exit\n", time(), getpid());
    task_die(0);
    assert(0, "task_die should have stopped execution");
}
