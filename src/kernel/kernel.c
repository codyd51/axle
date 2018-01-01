//freestanding headers
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//kernel stdlib headers
#include <std/printf.h>
#include <std/string.h>

//kernel headers
#include <kernel/drivers/vga_screen/vga_screen.h>
#include <kernel/multiboot.h>
#include <kernel/boot.h>

#define NotImplemented() do {_assert("Not implemented", __FILE__, __LINE__);} while(0);
#define assert(msg) do {_assert(msg, __FILE__, __LINE__);} while(0);

void _assert(const char* msg, const char* file, int line) {
	printf("Kernel assertion. %s, line %d: %s\n", file, line, msg);
	asm("cli");
	while (1) {}
}


size_t strlen(const char* str) {
	size_t len = 0;
	while (str[len] != '\0') {
		len++;
	}
	return len;
}

void kernel_main(void) {
void kernel_main(multiboot_info* mboot_data) {
	vga_screen_init();


	printf("int %d float %f str %s ch %c ptr %p\n", 5, 3.14, "hello", 'x', 0xdeadbeef);
	printf("%c\n", 'd');
	printf("hello %s\n", "world");
}
