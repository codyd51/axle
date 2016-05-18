#include "gfx.h"
#include <std/std.h>
#include <kernel/kernel.h>
#include <kernel/drivers/pit/timer.h>
#include <gfx/font/font.h>
#include "shapes.h"

#define VRAM_START 0xA0000

screen_t* get_vga_screen() {
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
	screen_t* screen = get_vga_screen();
	fill_screen(screen, 0);

	coordinate p1 = create_coordinate(screen->width / 2, screen->height * 0.25);
	coordinate p2 = create_coordinate(screen->width / 2 - 25, screen->height * 0.25 + 50);
	coordinate p3 = create_coordinate(screen->width / 2 + 25, screen->height * 0.25 + 50);
	triangle triangle = create_triangle(p1, p2, p3);
	draw_triangle(screen, triangle, 10);

	circle circle = create_circle(p1, 10);
	draw_circle(screen, circle, 14);

	font_t* font_map = setup_font();
	int y = screen->height * 0.6;
	draw_char(screen, font_map, 'a', screen->width / 2 - 35, y);
	draw_char(screen, font_map, 'x', screen->width / 2 - 25, y);
	draw_char(screen, font_map, 'l', screen->width / 2 - 15, y);
	draw_char(screen, font_map, 'e', screen->width / 2 - 5, y);
	draw_char(screen, font_map, 'o', screen->width / 2 + 15, y);
	draw_char(screen, font_map, 's', screen->width / 2 + 25, y);
	
	sleep(2000);
	switch_to_text();
}
