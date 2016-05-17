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

void wait_keypress() {
	regs16_t regs;
	regs.ax = 0x0000;
	int32(0x10, &regs);
}

void putpixel(screen_t* screen, int x, int y, int color) {
	unsigned loc = VRAM_START + (y * screen->width) + x;
	memset(loc, color, 1);
}

void fill_screen(screen_t* screen, int color) {
	memset((char*)VRAM_START, color, (screen->width * screen->height));
}

//calling putpixel directly will always be slow
//since we have to calculate where the pixel goes for every single pixel we place
//suffix _slow, replace this with faster function in future
void fillrect_slow(screen_t* screen, int x, int y, int w, int h, int color) {
	for (int i = y; i < h; i++) {
		for (int j = x; j < w; j++) {
			putpixel(screen, j, i, color);
		}
	}
}

void hline_slow(screen_t* screen, int x, int y, int w, int color) {
	for (; x < w; x++) {
	       putpixel(screen, x, y, color);
	}
}	

void vline_slow(screen_t* screen, int x, int y, int h, int color) {
	for (; y < h; y++) {
		putpixel(screen, x, y, color);
	}
}

void gfx_test() {
	screen_t* screen = get_vga_screen();
	fill_screen(screen, 0);

	//coordinate origin = create_coordinate(50, 50);
	//size size = create_size(50, 50);
	//rect rect = create_rect(origin, size);
	//draw_rect(screen, rect, 7);
	
	//coordinate p1 = create_coordinate(0, 0);
	//coordinate p2 = create_coordinate(50, 50);
	//line line = create_line(p1, p2);
	//draw_line(screen, line, 7);

	coordinate p1 = create_coordinate(screen->width / 2, screen->height * 0.25);
	coordinate p2 = create_coordinate(screen->width / 2 - 25, screen->height * 0.25 + 50);
	coordinate p3 = create_coordinate(screen->width / 2 + 25, screen->height * 0.25 + 50);
	triangle triangle = create_triangle(p1, p2, p3);
	draw_triangle(screen, triangle, 10);

	//fillrect(screen, 50, 5, 100, 10, 6);
	//hline_slow(screen, 80, 30, 200, 7);
	//vline_slow(screen, 25, 5, 150, 9);
	
	font_t* font_map = setup_font();
	int y = screen->height * 0.6;
	draw_char(screen, font_map, 'a', screen->width / 2 - 35, y);
	draw_char(screen, font_map, 'x', screen->width / 2 - 25, y);
	draw_char(screen, font_map, 'l', screen->width / 2 - 15, y);
	draw_char(screen, font_map, 'e', screen->width / 2 - 5, y);
	draw_char(screen, font_map, 'o', screen->width / 2 + 15, y);
	draw_char(screen, font_map, 's', screen->width / 2 + 25, y);

	//draw_string(screen, font_map, "AXLE OS", 0, 0);
	sleep(5000);
	getchar();
	switch_to_text();
/*
	printf_info("coordinate: %x x: %d y: %d", origin, origin->x, origin->y);
	printf_info("size: %x w: %d h %d", size, size->w, size->h);
	printf_info("rect %x", rect);
*/
}

void boot_screen() {
	screen_t* screen = get_vga_screen();
	fill_screen(screen, 0);

	coordinate p1 = create_coordinate(screen->width / 2, screen->height * 0.25);
	coordinate p2 = create_coordinate(screen->width / 2 - 25, screen->height * 0.25 + 50);
	coordinate p3 = create_coordinate(screen->width / 2 + 25, screen->height * 0.25 + 50);
	triangle triangle = create_triangle(p1, p2, p3);
	draw_triangle(screen, triangle, 10);

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
