#include "vga.h"
#include <gfx/lib/gfx.h>
#include <kernel/kernel.h>

void vga_screen_refresh(Screen* screen) {
	write_screen(screen);
}

Screen* switch_to_vga() {
	kernel_begin_critical();

	int width = 320;
	int height = 200;
	
	Screen* screen = screen_create(size_make(width, height), (uint32_t*)VRAM_START, VGA_DEPTH);
	process_gfx_switch(screen, VGA_DEPTH);

	regs16_t regs;
	regs.ax = 0x0013;
	int32(0x10, &regs);

	kernel_end_critical();

	return screen;
}
