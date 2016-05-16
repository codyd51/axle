#include <kernel/kernel.h>
#include <user/shell/shell.h>
#include <kernel/drivers/rtc/clock.h>
#include <std/common.h>
#include <kernel/util/paging/descriptor_tables.h>
#include <kernel/drivers/pit/timer.h>
#include <kernel/util/paging/paging.h>
#include <stdarg.h>
#include <gfx/lib/gfx.h>
#include <kernel/drivers/vesa/vesa.h>
#include <std/kheap.h>
#include <tests/test.h>

void print_os_name() {
	terminal_settextcolor(COLOR_GREEN);
	printf("[");
	terminal_settextcolor(COLOR_LIGHT_CYAN);
	printf("AXLE OS v");
	terminal_settextcolor(COLOR_LIGHT_RED);
	printf("0.3.0");
	terminal_settextcolor(COLOR_GREEN);
	printf("]\n");
}

#if defined(__cplusplus)
extern "C" //use C linkage for kernel_main
#endif
void kernel_main() {
	//initialize terminal interface
	terminal_initialize();

	//introductory message
	print_os_name();
	
	//run color test
	test_colors();

	//set up software interrupts
	printf_info("Initializing descriptor tables...");
	init_descriptor_tables();
	test_interrupts();

	printf_info("Initializing PIC timer...");
	init_timer(1000);

	printf_info("Initializing keyboard driver...");
	init_kb();

	printf_info("Initializing paging...");
	initialize_paging();

	test_heap();

	//force_page_fault();
	//force_hardware_irq();

	//wait for user to start shell
	printf("Kernel has finished booting. Press any key to enter shell.\n");
	getchar();
	
	init_shell();
	int exit_status = 1;
	while (1) {
		shell();
	}

}


