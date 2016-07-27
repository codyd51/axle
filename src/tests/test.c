#include "test.h"
#include <std/std.h>
#include <kernel/drivers/terminal/terminal.h>
#include <kernel/drivers/vesa/vesa.h>

void test_colors() {
	printf_info("Testing colors...");
	for (int i = 0; i < 16; i++) {
		printf("\e[%d;@", i);
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

void test_malloc() {
	printf_info("Testing malloc limit...");
	for (int i = 0; i < 32; i++) {
		//printf_dbg("allocating %d bytes", pow(2, i));
		uint32_t tmp = kmalloc(4096);
		printf_dbg("freeing %x", tmp);
		kfree(tmp);
	}
}

void test_printf() {
	printf_info("Testing printf...");
	printf_info("int: %d | hex: %x | char: %c | str: %s | float: %f | %%", 126, 0x14B7, 'q', "test", 3.1415926);
}

void test_time_unique() {
	printf_info("Testing time_unique...");
	for (int i = 0; i < 100; i++) {
		static uint32_t last = 0;
		uint32_t current = time_unique();

		if (last == time_unique()) {
			//we find the number of times this stamp was encountered by
			//reverse engineering the clock slide
			//the slide is the slide stamp minus the real stamp
			printf_err("time_unique failed, stamp %u encountered %u times", time_unique(), time_unique() - time());
			return;
		}
		last = current;
	}
	printf_info("time_unique test passed");
}
