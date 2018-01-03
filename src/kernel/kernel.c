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
    uint32_t frame1 = pmm_alloc();
    //uint32_t frame2 = pmm_alloc();
    //uint32_t frame3 = pmm_alloc();
    //printf("PMM gave 0x%08x, 0x%08x, 0x%08x\n", frame1, frame2, frame3);
    pmm_dump();
}
