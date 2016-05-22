#include "gfx.h"
#include <std/std.h>
#include <kernel/kernel.h>
#include <kernel/drivers/pit/pit.h>
#include <gfx/font/font.h>
#include "shapes.h"
#include <std/std.h>
#include <tests/gfx_test.h>

#define VRAM_START 0xA0000

#define VGA_DEPTH 8 
#define VESA_DEPTH 24

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
	screen->window.size.width = width;
	screen->window.size.height = height;
	screen->window.subviewsCount = 0;
	screen->depth = VGA_DEPTH;
	screen->vmem = kmalloc(width * height * sizeof(char));
	screen->physbase = VRAM_START;

	//start refresh loop
	setup_vga_screen_refresh(screen, 16);

	return screen;
}

void switch_to_text(Screen* screen) {
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

void putpixel_vesa(Screen* screen, int x, int y, int RGB) {
		int offset = x * (screen->depth / 8) + y * (screen->window.size.width * (screen->depth / 8));

		screen->vmem[offset + 0] = RGB & 0xFF; //blue
		screen->vmem[offset + 1] = (RGB >> 8) & 0xFF; //green
		screen->vmem[offset + 2] = (RGB >> 16) & 0xFF; //red
}

void putpixel_vga(Screen* screen, int x, int y, int color) {
	uint16_t loc = ((y * screen->window.size.width) + x);
	screen->vmem[loc] = color;
}

void putpixel(Screen* screen, int x, int y, int color) {
	if (screen->depth == VGA_DEPTH) {
		//VGA mode
		putpixel_vga(screen, x, y, color);
	}
	else if (screen->depth == VESA_DEPTH) {
		//VESA mode
		putpixel_vesa(screen, x, y, color);
	}
}

typedef struct Color { char val[3]; } Color;
void fill_screen(Screen* screen, Color color) {
	for (int loc = 0; loc < (screen->window.size.width * screen->window.size.height * (screen->depth / 8)); loc += (screen->depth / 8)) {
		memcpy(&screen->vmem[loc], color.val, (screen->depth / 8) * sizeof(uint8_t));
	}
}

void write_screen(Screen* screen) {
	vsync();
	memcpy((char*)screen->physbase, screen->vmem, (screen->window.size.width * screen->window.size.height * (screen->depth / 8)));
}

void rainbow_animation(Screen* screen, rect r) {
	//ROY G BIV
	//int colors[] = {0xFF0000, 0xFF7000, 0xFFFF00, 0x00FF00, 0x0000FF, 0x4B0082, 0x9400D3};
	int colors[] = {4, 42, 44, 46, 1, 13, 34};
	for (int i = 0; i < 7; i++) {
		coordinate origin = create_coordinate(r.origin.x + (r.size.width / 7) * i, r.origin.y);
		size size = create_size((r.size.width / 7), r.size.height);
		rect seg = create_rect(origin, size);

		draw_rect(screen, seg, colors[i], THICKNESS_FILLED);
		sleep(500 / 7);
	}
}

void vga_boot_screen(Screen* screen) {
	Color color;
	color.val[0] = 0;
	fill_screen(screen, color);

	coordinate p1 = create_coordinate(screen->window.size.width / 2, screen->window.size.height * 0.25);
	coordinate p2 = create_coordinate(screen->window.size.width / 2 - 25, screen->window.size.height * 0.25 + 50);
	coordinate p3 = create_coordinate(screen->window.size.width / 2 + 25, screen->window.size.height * 0.25 + 50);
	triangle triangle = create_triangle(p1, p2, p3);
	draw_triangle(screen, triangle, 2, 5);

	//Font* font_map = setup_font();
	//draw_string(screen, font_map, "axle os", screen->width / 2 - 35, screen->height * 0.6, 2);

	float rect_length = screen->window.size.width / 3;
	coordinate origin = create_coordinate((screen->window.size.width/2) - (rect_length / 2), screen->window.size.height / 4 * 3);
	size sz = create_size(rect_length - 5, screen->window.size.height / 16);
	rect border_rect = create_rect(origin, sz);

	//fill the rectangle with white initially
	draw_rect(screen, border_rect, 15, 1);

	sleep(1000);

	coordinate rainbow_origin = create_coordinate(origin.x + 2, origin.y + 2);
	size rainbow_size = create_size(rect_length - 4, sz.height - 3);
	rect rainbow_rect = create_rect(rainbow_origin, rainbow_size);
	rainbow_animation(screen, rainbow_rect);    

	sleep(250);
}
