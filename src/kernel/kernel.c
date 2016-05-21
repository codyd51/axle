#include <kernel/kernel.h>
#include <user/shell/shell.h>
#include <kernel/drivers/rtc/clock.h>
#include <std/common.h>
#include <kernel/util/paging/descriptor_tables.h>
#include <kernel/drivers/pit/pit.h>
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

void shell_loop() {
	int exit_status = 0;
	while (!exit_status) {
		exit_status = shell();
	}

	//give them a chance to recover
	for (int i = 5; i > 0; i--) {
		terminal_clear();
		printf_info("Shutting down in %d. Press any key to cancel", i);
		sleep(1000);
		if (haskey()) {
			//clear buffer
			while (haskey()) {
				getchar();
			}
			//restart shell loop
			terminal_clear();
			shell_loop();
			break;
		}
	}
	
	//we're dead
	terminal_clear();
} 

void kernel_begin_critical() {
	//disable interrupts while critical code executes
	asm ("cli");
}

void kernel_end_critical() {
	//reenable interrupts now that a critical section is complete
	asm ("sti");
}

void info_panel_refresh() {
	cursor pos = get_cursor();

	//set cursor near top left, leaving space to write
	cursor curs;
	curs.x = 65;
	curs.y = 0;
	set_cursor(curs);

	printf("PIT: %d", tick_count());
	//using \n would move cursor x = 0
	//instead, manually set to next row
	curs.y += 1;
	set_cursor(curs);
	printf("RTC: %d", time());

	//now that we're done, put the cursor back
	set_cursor(pos);
}

void initialize_info_panel() {
	timer_callback info_callback = add_callback(info_panel_refresh, 1, 1, NULL);
}

extern uint32_t placement_address;
uint32_t initial_esp;

#if defined(__cplusplus)
extern "C" //use C linkage for kernel_main
#endif
void kernel_main(struct multiboot* mboot_ptr, uint32_t initial_stack) {
	initial_esp = initial_stack;	

	//initialize terminal interface
	terminal_initialize();	

	//set up software interrupts
	printf_info("Initializing descriptor tables...");
	init_descriptor_tables();
	test_interrupts();

	printf_info("Initializing PIC timer...");
	init_timer(1000);

	printf_info("Initializing paging...");
	initialize_paging();
	
	//display booting screen
	boot_screen();

	//introductory message
	print_os_name();
	
	//run color test
	test_colors();

	printf_info("Initializing keyboard driver...");
	init_kb();

	test_heap();	

	//set up info panel
	initialize_info_panel();

	//force_page_fault();
	//force_hardware_irq();
	
	//wait for user to start shell
	printf("Kernel has finished booting. Press any key to enter shell.\n");
	getchar();
	
	init_shell();
	shell_loop(); 
}


