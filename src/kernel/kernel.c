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
#include <kernel/pmm/pmm.h>
#include <kernel/vmm/vmm.h>
#include <std/kheap.h>
#include <kernel/syscall/syscall.h>

//testing!
#include <kernel/multitasking/tasks/task_small.h>
#include <kernel/util/amc/amc.h>
#include <kernel/util/vfs/fs.h>

#define SPIN while (1) {sys_yield(RUNNABLE);}
#define SPIN_NOMULTI do {} while (1);

void print_os_name() {
    NotImplemented();
}

void system_mem() {
    NotImplemented();
}

static void kernel_spinloop() {
    printf("\nBoot complete, kernel spinlooping.\n");
    asm("cli");
    asm("hlt");
}

static void kernel_idle() {
    while (1) {
        //nothing to do!
        //put the CPU to sleep until the next interrupt
        asm volatile("hlt");
    }
}

static void awm_init() {
    const char* program_name = "awm";

    // VESA Framebuffer,
    boot_info_t* info = boot_info_get();
    vmm_identity_map_region(vmm_active_pdir(), info->framebuffer.address, info->framebuffer.size);

    FILE* fp = initrd_fopen(program_name, "rb");
    // Pass a pointer to the VESA linear framebuffer in argv
    // Not great, but kind of funny :-)
    // Later, framebuffer info can be communicated to awm via 
    // an init amc message.
    framebuffer_info_t framebuffer_info = boot_info_get()->framebuffer; 
    char* ptr = kmalloc(32);
    snprintf(ptr, 32, "0x%08x", &(boot_info_get()->framebuffer));
    char* argv[] = {program_name, ptr, NULL};

    elf_load_file(program_name, fp, argv);
    panic("noreturn");
}

static void cat() {
    const char* program_name = "cat";

    FILE* fp = initrd_fopen(program_name, "rb");
    char* argv[] = {program_name, "test-file.txt", NULL};
    elf_load_file(program_name, fp, argv);
    panic("noreturn");
}

static void window() {
    const char* program_name = "window";

    FILE* fp = initrd_fopen(program_name, "rb");
    char* argv[] = {program_name, NULL};
    elf_load_file(program_name, fp, argv);
    panic("noreturn");
}

static void rainbow() {
    const char* program_name = "rainbow";

    FILE* fp = initrd_fopen(program_name, "rb");
    char* argv[] = {program_name, NULL};
    elf_load_file(program_name, fp, argv);
    panic("noreturn");
}

static void tty_init() {
    const char* program_name = "tty";
    FILE* fp = initrd_fopen(program_name, "rb");
    char* argv[] = {program_name, NULL};
    elf_load_file(program_name, fp, argv);
	panic("noreturn");
	uint8_t scancode = inb(0x60);
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
    // Set this to 1ms to cause issues with keystrokes being lost
    // TODO(PT): Currently, a task-switch is a side-effect of a PIT ISR
    // In the future, work like scheduling should be done outside the ISR
    pit_timer_init(PIT_TICK_GRANULARITY_50MS);

    serial_init();

    // Kernel features
    pmm_init();
    pmm_dump();
    vmm_init();
    vmm_notify_shared_kernel_memory_allocated();
    syscall_init();
    initrd_init();

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
    task_spawn(ps2_keyboard_driver_launch, PRIORITY_DRIVER, "");
    task_spawn(ps2_mouse_driver_launch, PRIORITY_DRIVER, "");
    //task_spawn(tty_init);
    task_spawn(awm_init, PRIORITY_GUI, "");

    //task_spawn(cat);
    //task_spawn(rainbow);
    //task_spawn(window);

    // Bootstrapping complete - kill this process
    printf("[t = %d] Bootstrap task [PID %d] will exit\n", time(), getpid());
    task_die(0);
    assert(0, "task_die should have stopped execution");
}
