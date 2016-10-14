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
#include <user/xserv/xserv.h>
#include "multiboot.h"
#include <gfx/font/font.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <gfx/lib/view.h>
#include <kernel/util/syscall/syscall.h>
#include <kernel/util/mutex/mutex.h>
#include <std/printf.h>
#include <kernel/util/vfs/initrd.h>
#include <kernel/drivers/pci/pci_detect.h>

void print_os_name(void) {
	printf("\e[10;[\e[11;AXLE OS v\e[12;0.5.0\e[10;]\n");
}

void shell_loop(void) {
	int exit_status = 0;
	while (!exit_status) {
		exit_status = shell();
	}
/*
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
*/
	
	//we're dead
	terminal_clear();
}

/*
void kernel_begin_critical(void) {
	//disable interrupts while critical code executes
	asm ("cli");
}

void kernel_end_critical(void) {
	//reenable interrupts now that a critical section is complete
	asm ("sti");
}
*/

void info_panel_refresh(void) {
	/*
	term_cursor pos = terminal_getcursor();

	//set cursor near top right, leaving space to write
	term_cursor curs = (term_cursor){65, 0};
	terminal_setcursor(curs);

	printf("PIT: %d", tick_count());
	//using \n would move cursor x = 0
	//instead, manually set to next row
	curs.y += 1;
	terminal_setcursor(curs);
	printf("RTC: %d", time());

	//now that we're done, put the cursor back
	terminal_setcursor(pos);
	*/
}

void info_panel_install(void) {
	printf_info("Installing text-mode info panel...");
	//timer_callback info_callback = add_callback(info_panel_refresh, 1, 1, NULL);
}

extern uint32_t placement_address;
uint32_t initial_esp;

uint32_t module_detect(multiboot* mboot_ptr) {
	printf_info("Detected %d GRUB modules", mboot_ptr->mods_count);
	ASSERT(mboot_ptr->mods_count > 0, "no GRUB modules detected");
	uint32_t initrd_loc = *((uint32_t*)mboot_ptr->mods_addr);
	uint32_t initrd_end = *(uint32_t*)(mboot_ptr->mods_addr+4);
	//don't trample modules
	placement_address = initrd_end;
	return initrd_loc;
}

void kernel_main(multiboot* mboot_ptr, uint32_t initial_stack) {
	initial_esp = initial_stack;

	//initialize terminal interface
	terminal_initialize();

	//introductory message
	print_os_name();
	test_colors();

	printf_info("Available memory:");
	printf("%d -> %dMB\n", mboot_ptr->mem_upper, (mboot_ptr->mem_upper/1024));

	//descriptor tables
	gdt_install();
	idt_install();
	
	test_interrupts();

	//timer driver (many functions depend on timer interrupt so start early)
	pit_install(1000);

	//find any loaded grub modules
	uint32_t initrd_loc = module_detect(mboot_ptr);

	//utilities
	paging_install();
	sys_install();
	//tasking_install(PRIORITIZE_INTERACTIVE);
	tasking_install(LOW_LATENCY);

	//drivers
	kb_install();
	mouse_install();
	pci_install();
   
	//initialize initrd, and set as fs root
	fs_root = initrd_install(initrd_loc);

	//test facilities
	test_heap();
	test_printf();
	test_time_unique();
	test_malloc();
	test_crypto();

	if (!fork("shell")) {
		//start shell 
		shell_init();
		shell_loop();
	}

	if (!fork("sleepy")) {
		sleep(20000);
		printf_dbg("Sleepy thread slept!");
		_kill();
	}

	_kill();

	//this should never be reached as the above call is never executed
	//if for some reason it is, just spin
	while (1) {
		sys_yield();
	}

	//if by some act of god we've reached this point, just give up and assert
	ASSERT(0, "Kernel exited");
}


