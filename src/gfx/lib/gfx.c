#include "gfx.h"
#include <std/std.h>
#include <kernel/kernel.h>
#include <kernel/drivers/pit/pit.h>
#include <gfx/font/font.h>
#include "shapes.h"
#include <std/std.h>
#include <tests/gfx_test.h>

#define VRAM_START 0xA0000

void screen_refresh(screen_t* screen) {
	write_screen(screen);
}

void setup_screen_refresh(screen_t* screen, double interval) {
	screen->callback_index = add_callback(screen_refresh, interval, true, screen);
}

screen_t* switch_to_vga() {
	regs16_t regs;
	regs.ax = 0x0013;
	int32(0x10, &regs);

	int width = 320;
	int height = 200;

	screen_t* screen = (screen_t*)kmalloc(sizeof(screen_t));
	screen->width = width;
	screen->height = height;
	screen->depth = 256;
	screen->vmem = kmalloc(width * height * sizeof(char));

	//start refresh loop
	setup_screen_refresh(screen, 16);

	return screen;
}

void switch_to_text(screen_t* screen) {
	//stop refresh loop for this screen
	remove_callback_at_index(screen->callback_index);

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
	uint16_t loc = ((y * screen->width) + x);
	screen->vmem[loc] = color;
}

void fill_screen(screen_t* screen, int color) {
	memset(screen->vmem, color, (screen->width * screen->height));
}

void write_screen(screen_t* screen) {
	memcpy((char*)VRAM_START, screen->vmem, (screen->width * screen->height));
}

void boot_screen() {
	screen_t* screen = switch_to_vga();
	fill_screen(screen, 0);

	coordinate p1 = create_coordinate(screen->width / 2, screen->height * 0.25);
	coordinate p2 = create_coordinate(screen->width / 2 - 25, screen->height * 0.25 + 50);
	coordinate p3 = create_coordinate(screen->width / 2 + 25, screen->height * 0.25 + 50);
	triangle triangle = create_triangle(p1, p2, p3);
	draw_triangle(screen, triangle, 10, -1);

	font_t* font_map = setup_font();
	draw_string(screen, font_map, "axle os", screen->width / 2 - 35, screen->height * 0.6, 2);

	sleep(2000);
	switch_to_text(screen);
}
