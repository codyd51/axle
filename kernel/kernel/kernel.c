//freestanding headers
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//kernel stdlib headers
#include <std/printf.h>
#include <std/string.h>

//kernel headers
#include <kernel/multiboot.h>
#include <kernel/boot.h>
#include <kernel/elf.h>
#include <kernel/assert.h>
#include <kernel/boot_info.h>
#include <kernel/segmentation/gdt.h>
#include <kernel/interrupts/interrupts.h>

//kernel drivers
#include <kernel/drivers/kb/kb.h>
#include <kernel/drivers/mouse/mouse.h>
#include <kernel/drivers/pit/pit.h>
#include <kernel/drivers/serial/serial.h>
#include <kernel/drivers/text_mode/text_mode.h>

//higher-level kernel features
#include <std/kheap.h>
#include <kernel/pmm/pmm.h>
#include <kernel/vmm/vmm.h>
#include <kernel/syscall/syscall.h>

//testing!
#include <kernel/multitasking/tasks/task_small.h>
#include <kernel/util/amc/amc.h>

#define SPIN while (1) {sys_yield(RUNNABLE);}
#define SPIN_NOMULTI do {} while (1);

void print_os_name() {
    NotImplemented();
}

void system_mem() {
    NotImplemented();
}

static void kernel_idle() {
    while (1) {
        //nothing to do!
        //put the CPU to sleep until the next interrupt
        asm volatile("hlt");
    }
}

#include <kernel/util/vfs/vfs.h>
static void _launch_program(const char* program_name, uint32_t arg2, uint32_t arg3) {
    printf("_launch_program(%s, 0x%08x 0x%08x)\n", program_name, arg2, arg3);
    char* argv[] = {program_name, NULL};

    initrd_fs_node_t* node = vfs_find_initrd_node_by_name(program_name);
    uint32_t address = node->initrd_offset;
	elf_load_buffer(program_name, address, node->size, argv);
	panic("noreturn");
}

uint32_t initial_esp = 0;
void kernel_main(struct multiboot_info* mboot_ptr, uint32_t initial_stack) {
    initial_esp = initial_stack;
    text_mode_init();

    // Environment info
    boot_info_read(mboot_ptr);
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
    vmm_init();
    vmm_notify_shared_kernel_memory_allocated();
    syscall_init();
    vfs_init();

    // We've now allocated all the kernel memory that'll be mapped into every process 
    // Inform the VMM so that it can begin allocating memory outside of shared page tables
    vmm_dump(boot_info_get()->vmm_kernel);

    // Higher-level features like multitasking
    tasking_init();

    // Initialize PS/2 controller
    // (and sub-drivers, such as a PS/2 keyboard and mouse)
    ps2_controller_init();

    // Early boot is finished
    // Multitasking and program loading is now available

    // Launch some initial drivers and services
    const char* launch_programs[] = {
        // System services
        "kb_driver",
        "mouse_driver",
        "ata_driver",
        "awm",
        // Higher-level facilities
        //"tty",
        //"pci_driver",
        //"net",
        //"netclient",
        // User applications
        "file_manager",
        //"image_viewer",
        // Games
        //"breakout",
        //"snake",
        //"2048",
    };
    for (uint32_t i = 0; i < sizeof(launch_programs) / sizeof(launch_programs[0]); i++) {
        const char* program_name = launch_programs[i];
        task_spawn__with_args(_launch_program, program_name, 0, 0, "");
    }

    // Bootstrapping complete - kill this process
    printf("[t = %d] Bootstrap task [PID %d] will exit\n", time(), getpid());
    task_die(0);
    assert(0, "task_die should have stopped execution");
}
