#include "test.h"
#include <std/std.h>
#include <kernel/drivers/vga/vga.h>
#include <kernel/drivers/vesa/vesa.h>

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

