#include "gfx.h"
#include <std/std.h>
#include <kernel/kernel.h>
#include <kernel/drivers/pit/timer.h>
#include <gfx/font/font.h>
#include "shapes.h"

#define VRAM_START 0xA0000

screen_t* switch_to_vga() {
	regs16_t regs;
	regs.ax = 0x0013;
	int32(0x10, &regs);

	screen_t* screen = (screen_t*)kmalloc(sizeof(screen_t));
	screen->width = 320;
	screen->height = 200;
	screen->depth = 256;
	return screen;
}

void switch_to_text() {
	regs16_t regs;
	regs.ax = 0x0003;
	int32(0x10, &regs);
}

void vsync() {
	//wait until previous retrace has ended
	do {} while (inb(0x3DA) & 8);

	//wait until new trace has just begun
	do {} while (!(inb(0x3DA) & 8));
}

void putpixel(screen_t* screen, int x, int y, int color) {
	unsigned loc = VRAM_START + (y * screen->width) + x;
	memset(loc, color, 1);
}

void fill_screen(screen_t* screen, int color) {
	memset((char*)VRAM_START, color, (screen->width * screen->height));
}

void boot_screen() {
	screen_t* screen = switch_to_vga();
	fill_screen(screen, 0);

	coordinate p1 = create_coordinate(screen->width / 2, screen->height * 0.25);
	coordinate p2 = create_coordinate(screen->width / 2 - 25, screen->height * 0.25 + 50);
	coordinate p3 = create_coordinate(screen->width / 2 + 25, screen->height * 0.25 + 50);
	triangle triangle = create_triangle(p1, p2, p3);
	draw_triangle(screen, triangle, 10);

	font_t* font_map = setup_font();
	draw_string(screen, font_map, "axle os", screen->width / 2 - 35, screen->height * 0.6);
	
	sleep(2000);
	switch_to_text();
}
