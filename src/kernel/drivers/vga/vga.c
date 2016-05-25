#include "vga.h"

void vga_screen_refresh(Screen* screen) {
	write_screen(screen);
}

void setup_vga_screen_refresh(Screen* screen, double interval) {
	screen->callback = add_callback(vga_screen_refresh, interval, true, screen);
}

Screen* switch_to_vga() {
	regs16_t regs;
	regs.ax = 0x0013;
	int32(0x10, &regs);

	int width = 320;
	int height = 200;

	Screen* screen = (Screen*)kmalloc(sizeof(Screen));
	screen->window->size.width = width;
	screen->window->size.height = height;
	screen->depth = VGA_DEPTH;
	screen->vmem = kmalloc(width * height * sizeof(char));
	screen->physbase = VRAM_START;

	//start refresh loop
	setup_vga_screen_refresh(screen, 16);

	return screen;
}
