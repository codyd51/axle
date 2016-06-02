#include "vga.h"
#include <gfx/lib/shapes.h>

void vga_screen_refresh(Screen* screen) {
	write_screen(screen);
}

void setup_vga_screen_refresh(Screen* screen, double interval) {
	screen->callback = add_callback(vga_screen_refresh, interval, true, screen);
}

Screen* switch_to_vga() {
	kernel_begin_critical();

	int width = 320;
	int height = 200;

	Screen* screen = (Screen*)kmalloc(sizeof(Screen));
	screen->window = create_window(rect_make(point_make(0, 0), size_make(width, height)));
	screen->depth = VGA_DEPTH;
	screen->vmem = kmalloc(width * height * sizeof(uint8_t));
	screen->physbase = VRAM_START;
	screen->font = setup_font();

	regs16_t regs;
	regs.ax = 0x0013;
	int32(0x10, &regs);
	
	//start refresh loop
	setup_vga_screen_refresh(screen, 33);

	kernel_end_critical();

	return screen;
}

void putpixel_vga(Screen* screen, int x, int y, Color color) {
	uint16_t loc = ((y * screen->window->size.width) + x);
	screen->vmem[loc] = color.val[0];
}
