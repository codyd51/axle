#include "vga.h"
#include <gfx/lib/shapes.h>
#include <kernel/kernel.h>

Window* create_window_int(Rect frame, bool root);

void vga_screen_refresh(Screen* screen) {
	write_screen(screen);
}

void setup_vga_screen_refresh(Screen* screen, double interval) {
	screen->callback = add_callback((void*)vga_screen_refresh, interval, true, screen);
}

Screen* switch_to_vga() {
	kernel_begin_critical();

	int width = 320;
	int height = 200;
	
	process_gfx_switch(VGA_DEPTH);

	Screen* screen = (Screen*)kmalloc(sizeof(Screen));
	screen->window = create_window_int(rect_make(point_make(0, 0), size_make(width, height)), true);
	screen->depth = VGA_DEPTH;
	screen->bpp = 1;

	screen->physbase = (uint8_t*)VRAM_START;

	regs16_t regs;
	regs.ax = 0x0013;
	int32(0x10, &regs);

	kernel_end_critical();

	return screen;
}
