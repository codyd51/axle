#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "drivers/vga_screen/vga_screen.h"

size_t strlen(const char* str) {
	size_t len = 0;
	while (str[len] != '\0') {
		len++;
	}
	return len;
}


void kernel_main(void) {
	vga_screen_init();
	vga_screen_puts("Hello world!\n");
}
