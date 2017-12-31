#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/drivers/vga_screen/vga_screen.h>
#include <kernel/multiboot.h>
#include <std/printf.h>

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
