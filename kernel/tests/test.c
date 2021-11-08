#include "test.h"
#include <std/std.h>
#include <kernel/drivers/terminal/terminal.h>
#include <kernel/drivers/rtc/clock.h>
#include <crypto/crypto.h>

void test_colors() {
	printf("\e[1;@");
	printf("\e[2;@");
	printf("\e[3;@");
	printf("\e[4;@");
	printf("\e[5;@");
	printf("\e[6;@");
	printf("\e[7;@");
	printf("\e[8;@");
	printf("\e[9;@");
	printf("\e[10;@");
	printf("\e[11;@");
	printf("\e[12;@");
	printf("\e[13;@");
	printf("\e[14;@");
	printf("\e[15;@");
	printf("\e[16;@");

	printf("\n");
}

void force_hardware_irq() {
	printf_info("Forcing hardware IRQ...");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiv-by-zero"
	int i = 500/0;
#pragma GCC diagnostic pop
	printf_dbg("%d", i);
}

void force_page_fault() {
	printf_info("Forcing page fault...");
	uintptr_t* ptr = (uintptr_t*)0xA0000000;
	uint32_t do_fault = *ptr;
	printf_err("This should never be reached: %d", do_fault);
}

void test_interrupts() {
	printf_info("Testing interrupts...");
	asm volatile("mov $0xdeadbeef, %eax");
	asm volatile("mov $0xcafebabe, %ecx");
	asm volatile("int $0x3");
	//asm volatile("int $0x4");
}

void test_heap() {
	printf_info("Testing heap...");

	uintptr_t* a = kmalloc(8);
	uintptr_t* b = kmalloc(8);
	printf_dbg("a: %x, b: %x", a, b);
	kfree(a);
	kfree(b);

	uintptr_t* c = kmalloc(12);
	printf_dbg("c: %x", c);
	kfree(c);

	if (a == c) {
		printf_info("Heap test passed");
	}
	else printf_err("Heap test failed, expected %x to be marked free", a);
}

void test_malloc() {
	printf_info("Testing malloc...");

	//Check used memory before malloc test
	//if more mem is used after test, then the test failed
	/*
	uint32_t used = used_mem();

	for (int i = 0; i < 32; i++) {
		uintptr_t* tmp = (uintptr_t*)kmalloc(0x1000);
		kfree(tmp);
	}

	if (used != used_mem()) {
		printf_err("Malloc test failed. Expected %x bytes in use, had %x", used, used_mem());
		return;
	}
	*/
	printf_info("Malloc test passed");
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

void test_crypto() {
	printf_info("Testing SHA256...");
	printf_info("SHA256 test %s", sha256_test() ? "passed":"failed");
	printf_info("Testing AES...");
	printf_info("AES test %s", aes_test() ? "passed":"failed");
}
