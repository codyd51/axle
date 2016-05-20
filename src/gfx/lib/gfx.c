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
	screen->callback = add_callback(screen_refresh, interval, true, screen);
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
	remove_callback(screen->callback);

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

void rainbow_animation(screen_t* screen, rect r) {
	//ROY G BIV
	int colors[] = {4, 42, 44, 46, 1, 13, 34};
	for (int i = 0; i < 7; i++) {
		coordinate origin = create_coordinate(r.origin.x + (r.size.w / 7) * i, r.origin.y);
		size size = create_size((r.size.w / 7), r.size.h);
		rect seg = create_rect(origin, size);

		draw_rect(screen, seg, colors[i], THICKNESS_FILLED);
		sleep(500 / 7);
	}
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

	float rect_length = screen->width / 4;
	coordinate origin = create_coordinate((screen->width/2) - (rect_length / 2), screen->height / 4 * 3 - 1);
	size sz = create_size(rect_length, screen->height / 8 + 2);
	rect border_rect = create_rect(origin, sz);

	//fill the rectangle with white initially
	draw_rect(screen, border_rect, 15, 1);

	sleep(1000);

	coordinate rainbow_origin = create_coordinate(origin.x + 2, origin.y + 2);
	size rainbow_size = create_size(sz.w - 3, sz.h - 3);
	rect rainbow_rect = create_rect(rainbow_origin, rainbow_size);
	rainbow_animation(screen, rainbow_rect);    

	sleep(250);
	switch_to_text(screen);
}
