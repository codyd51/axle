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

uint8_t make_color(enum vga_color fg, enum vga_color bg) {
	return fg | bg << 4;
}

uint16_t make_vgaentry(char c, uint8_t color) {
	uint16_t c16 = c;
	uint16_t color16 = color;
	return c16 | color16 << 8;
}

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;

void terminal_initialize() {
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = make_color(COLOR_LIGHT_BLUE, COLOR_BLACK);
	terminal_buffer = VGA_MEM;

	terminal_clear();
}

void terminal_setcolor(uint8_t color) {
	terminal_color = color;
}

void terminal_settextcolor(enum vga_color col) {
	terminal_color = make_color(col, (terminal_color >> 4));
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = make_vgaentry(c, color);
}

void terminal_push_back_line() {
	terminal_column = 0;

    //move every character up by one row
	for (size_t row = 0; row < VGA_HEIGHT; row++) {
		for (size_t col = 0; col < VGA_WIDTH; col++) {
			size_t index = (row+1) * VGA_WIDTH + col;
			uint8_t color = terminal_buffer[index] >> 8;
			terminal_putentryat(terminal_buffer[index], color, col, row);
		}
	}
	terminal_row = VGA_HEIGHT-1;
}

//updates hardware cursor
void move_cursor() {
	//screen is 80 characters wide
	u16int cursorLocation = terminal_row * 80 + terminal_column;
	outb(0x3D4, 14); //tell VGA board we're setting high cursor byte
	outb(0x3D5, cursorLocation >> 8); //send high cursor byte
	outb(0x3D4, 15); //tell VGA board we're setting the low cursor byte
	outb(0x3D5, cursorLocation); //send low cursor byte
}

void terminal_putchar(char c) {
	//check for newline character
	if (c == '\n') {
		terminal_column = 0;
		if (++terminal_row >= VGA_HEIGHT) {
			terminal_push_back_line();
		}
	}
	//tab character
	else if (c == '\t') {
		terminal_column += 4;
	}
	else {
		terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
	}
	
	if (++terminal_column == VGA_WIDTH) {
		terminal_column = 0;

		if (++terminal_row == VGA_HEIGHT) {	
			terminal_push_back_line();
		}
	}

	move_cursor();
}

void terminal_removechar() {
	terminal_putentryat(' ', terminal_color, terminal_column-1, terminal_row);
	--terminal_column;
	move_cursor();
}

void terminal_writestring(const char* data) {
	size_t datalen = strlen(data);
	for (size_t i = 0; i < datalen; i++) {
		terminal_putchar(data[i]);
	}
}

void terminal_clear() {
	terminal_row = 0;
	terminal_column = 0;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = make_vgaentry(' ', terminal_color);
		}
	}
}

//defined in assembly
extern void enable_A20();

void enter_protected() {
	//disable interrupts
	//asm("cli");

	//wait for keyboard controller to clear
	//if bottom bit is set, kb is busy
	while (inb(0x64) & 1 != 0)
		printf_dbg("kb status: %d", inb(0x64));

	int i = enableA20();
	printf_dbg("return status: %d", i);
}

void test_colors() {
	printf_info("Testing colors...");
	for (int i = 0; i < 16; i++) {
		terminal_settextcolor(i);
		printf("@");
	}
	printf("\n");
}

void force_hardware_irq() {
	printf_info("Forcing hardware IRQ...");
	int i;
	i = 500/0;
	printf_dbg("%d", i);
}

void force_page_fault() {
	printf_info("Forcing page fault...");
	u32int* ptr = (u32int*)0xA0000000;
	u32int do_fault = *ptr;
	printf_err("This should never be reached: %d", do_fault);
}

void test_interrupts() {
	printf_info("Testing interrupts...");
	asm volatile("int $0x3");
	asm volatile("int $0x4");
}

void test_vesa() {
	printf_info("Testing VESA detection...");
	
	printf("Press any key to test graphics mode. Press any key to exit.\n");
	getchar();
	
	vesa_mode_info* mode_info = get_vesa_info();
	
	getchar();
	switch_to_text();
	printf_info("Back in text mode");

	printf_dbg("attributes: %d", mode_info->attributes);
	printf_dbg("granularity: %d", mode_info->granularity);
	printf_dbg("winsize: %d", mode_info->win_size);
	printf_dbg("pitch: %d", mode_info->pitch);
	printf_dbg("x_res: %d, y_res: %d", mode_info->x_res, mode_info->y_res);
	printf_dbg("red_base: %d, red_position: %d", mode_info->red_mask, mode_info->red_position);
	printf_dbg("physbase: %x", mode_info->physbase);
}

void test_heap() {
	printf_info("Testing heap's reallocation ability...");
	
	u32int a = kmalloc(8);
	u32int b = kmalloc(8);
	printf_dbg("a: %x, b: %x", a, b);
	
	printf_info("Freeing test values");
	kfree(a);
	kfree(b);

	u32int c = kmalloc(12);
	printf_dbg("c: %x", c);
	kfree(c);
}

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
	force_hardware_irq();

	//wait for user to start shell
	printf("Kernel has finished booting. Press any key to enter shell.\n");
	getchar();
	
	init_shell();
	int exit_status = 1;
	while (1) {
		shell();
	}

}


