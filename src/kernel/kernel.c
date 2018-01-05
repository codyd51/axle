#include "axle.h"
#include <kernel/drivers/text_mode/text_mode.h>
#include <kernel/assert.h>

#define SPIN while (1) {sys_yield(RUNNABLE);}
#define SPIN_NOMULTI do {} while (1);

void print_os_name() {
	NotImplemented();
}

void system_mem() {
	NotImplemented();
}

uint32_t initial_esp = 0;
void kernel_main(struct multiboot_info* mboot_ptr, uint32_t initial_stack) {
	initial_esp = initial_stack;
	text_mode_init();

	boot_info_read(mboot_ptr);
	boot_info_dump();
}
