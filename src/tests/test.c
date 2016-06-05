#include "test.h"
#include <std/std.h>
#include <kernel/drivers/terminal/terminal.h>
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
	uint32_t* ptr = (uint32_t*)0xA0000000;
	uint32_t do_fault = *ptr;
	printf_err("This should never be reached: %d", do_fault);
}

void test_interrupts() {
	printf_info("Testing interrupts...");
	asm volatile("mov $0xdeadbeef, %eax");
	asm volatile("mov $0xcafebabe, %edx");
	asm volatile("int $0x3");
	asm volatile("int $0x4");
}

void test_heap() {
	printf_info("Testing heap...");

	uint32_t a = kmalloc(8);
	uint32_t b = kmalloc(8);
	printf_dbg("a: %x, b: %x", a, b);
	kfree(a);
	kfree(b);

	uint32_t c = kmalloc(12);
	printf_dbg("c: %x", c);
	kfree(c);

	if (a == c) {
		printf_info("Heap test passed");
	}
	else printf_err("Heap test failed, expected %x to be marked free", a);
}
