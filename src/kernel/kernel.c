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
#include <kernel/util/multitasking/task.h>
#include <gfx/lib/view.h>
#include <kernel/util/syscall/syscall.h>

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
void kernel_main(multiboot* mboot_ptr, uint32_t initial_stack) {
	initial_esp = initial_stack;

	//initialize terminal interface
	terminal_initialize();	

	//introductory message
	print_os_name();
	
	//run color test
	test_colors();

	printf_info("Available memory:");
	printf("%d -> %dMB\n", mboot_ptr->mem_upper, (mboot_ptr->mem_upper/1024));
	
	//set up software interrupts
	printf_info("Initializing descriptor tables...");
	init_descriptor_tables();
	//test_interrupts();

	printf_info("Initializing PIC timer...");
	init_timer(1000);

	printf_info("Initializing paging...");
	initialize_paging();

	printf_info("Initializing syscalls...");
	initialize_syscalls();

	printf_info("Initializing tasking...");
	initialize_tasking();
	
	printf_info("Initializing keyboard driver...");
	init_kb();

	test_heap();	

	//set up info panel
	initialize_info_panel();

	//force_page_fault();
	//force_hardware_irq();	
	
	//give user a chance to stay in verbose boot
	printf("Press any key to stay in shell mode. Continuing in     ");
	for (int i = 3; i > 0; i--) {
		printf("\b\b\b\b%d...", i);
		sleep(1000);
		if (haskey()) {
			//clear buffer
			while (haskey()) {
				getchar();
			}
			
			init_shell();
			shell_loop();

			break;
		}
	}

	//switch into VGA for boot screen
	Screen* vga_screen = switch_to_vga();
	
	//display boot screen
	vga_boot_screen(vga_screen);

	gfx_teardown(vga_screen);
	switch_to_text();

	//switch to VESA for x serv
	Screen* vesa_screen = switch_to_vesa();
	
	Rect r = rect_make(point_make(50, 50), size_make(400, 500));
	Window* window = create_window(r);
	add_subwindow(vesa_screen->window, window);

	Rect image_frame = window->content_view->frame;
	uint32_t* bitmap = kmalloc(image_frame.size.width * image_frame.size.height * sizeof(uint32_t));
	
	for (int i = 0; i < (image_frame.size.width * image_frame.size.height); i++) {
		static uint32_t col = 0x0;
		bitmap[i] = col;
		col += 0x1;
	}
	
	Image* image = create_image(image_frame, bitmap);
	//add_subimage(window->content_view, image);
	
	Label* label = create_label(image_frame, "Lorem ipsum dolor sit amet consectetur apipiscing elit Donex purus arcu suscipit ed felis eu blandit blandit quam Donec finibus euismod lobortis Sed massa nunc malesuada ac ante eleifend dictum laoreet massa Aliquam nec dictum turpis pellentesque lacinia ligula Donec et tellus maximum dapibus justo auctor egestas sapien Integer venantis egesta malesdada Maecenas venenatis urna id posuere bibendum eros torto gravida ipsum sed tempor arcy andte ac odio Morbi elementum libero id velit bibendum auctor It sit amet ex eget urna venenatis laoreet Proin posuere urna nec ante tutum lobortis Cras nec elit tristique dolor congue eleifend");
	add_sublabel(window->content_view, label);

	while (1) {}
/*
	init_shell();
	shell_loop(); 
*/
}


