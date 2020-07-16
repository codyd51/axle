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

void drivers_init(void) {
    pit_timer_init(PIT_TICK_GRANULARITY_1MS);
    serial_init();
    mouse_install();
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

static void tty_init() {
    const char* program_name = "tty";
    FILE* fp = initrd_fopen(program_name, "rb");
    char* argv[] = {program_name, NULL};
    elf_load_file(program_name, fp, argv);
	panic("noreturn");
	uint8_t scancode = inb(0x60);
}

static void acker() {
    while (true) {
        //sleep(1000);
        sleep(20);
        char scancode = 'a';
        amc_message_t* amc_msg = amc_message_construct__from_core(&scancode, 1);
        amc_message_send("com.user.window", amc_msg);
    }
}

uint32_t initial_esp = 0;
void kernel_main(struct multiboot_info* mboot_ptr, uint32_t initial_stack) {
    initial_esp = initial_stack;
    //set up this driver first so we can output to framebuffer
    text_mode_init();

    //environment info
    boot_info_read(mboot_ptr);
    boot_info_dump();

    //x86 descriptor tables
    gdt_init();
    interrupt_init();

    //external device drivers
    drivers_init();

    //kernel features
    timer_init();
    pmm_init();
    pmm_dump();
    vmm_init();
    kheap_init();
    syscall_init();
    initrd_init();

    // We've now allocated all the kernel memory that'll be mapped into every process 
    // Inform the VMM so that it can begin allocating memory outside of shared page tables
    vmm_dump(boot_info_get()->vmm_kernel);
    vmm_notify_shared_kernel_memory_allocated();

    tasking_init();

    // Now that multitasking / program loading is available,
    // launch external services and drivers
    task_spawn(kb_init);
    task_spawn(awm_init);
    task_spawn(tty_init);

    sleep(1000);
    //task_spawn(cat);
    //task_spawn(acker);
    task_spawn(window);

    // Bootstrapping complete - kill this process
    printf("[t = %d] Bootstrap task [PID %d] will exit\n", time(), getpid());
    task_die(0);
    assert(0, "task_die should have killed us");
    kernel_idle();

    while (1) {}
    //the above call should never return, but just in case...
    kernel_spinloop();
}
