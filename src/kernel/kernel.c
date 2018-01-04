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
#include <kernel/assert.h>
#include <kernel/boot_info.h>


void kernel_main(struct multiboot_info* mboot_data) {
	vga_screen_init();

    //initialize system state
	boot_info_read(mboot_data);
	boot_info_dump();

	pmm_init();

    vga_screen_clear();

    printf("Allocating 4096 frames... \n");
    static uint32_t b[0x1000] = {0};
    for (int i = 0; i < 0x1000; i++) {
        uint32_t frame = pmm_alloc();
        b[i] = frame;
    }
    pmm_dump();

    printf("\nFreeing 4096 frames...\n");
    for (int i = 0; i < 0x1000; i++) {
        pmm_free(b[i]);
    }
    pmm_dump();

    printf("\nAllocating 1 frame\n");
    uint32_t frame = pmm_alloc();
    pmm_dump();
}
