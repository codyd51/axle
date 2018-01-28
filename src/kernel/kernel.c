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
#include <kernel/assert.h>
#include <kernel/boot_info.h>
#include <kernel/segmentation/gdt.h>
#include <kernel/interrupts/interrupts.h>

//kernel drivers
#include <kernel/drivers/pit/pit.h>
#include <kernel/drivers/text_mode/text_mode.h>

//higher-level kernel features
#include <kernel/pmm/pmm.h>
#include <kernel/vmm/vmm.h>
#include <std/kheap.h>
#include <kernel/util/syscall/syscall.h>

//testing!
#include <kernel/util/multitasking/tasks/task.h>

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
}

static void kernel_spinloop() {
    printf("\nBoot complete, kernel spinlooping.\n");
    asm("cli");
    asm("hlt");
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
	pmm_init();
    vmm_init();
    kheap_init();
    syscall_init();

    kernel_spinloop();
}
