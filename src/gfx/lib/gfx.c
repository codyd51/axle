#include "gfx.h"
#include <std/std.h>
#include <kernel/kernel.h>
#include <kernel/drivers/pit/pit.h>
#include <gfx/font/font.h>
#include "shapes.h"
#include <std/std.h>
#include <tests/gfx_test.h>
#include <kernel/drivers/vga/vga.h>
#include <kernel/drivers/vesa/vesa.h>

void gfx_teardown(Screen* screen) {
	//stop refresh loop for this screen
	remove_callback(screen->callback);

	//free screen
	kfree(screen->vmem);
	kfree(screen);
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

void putpixel(Screen* screen, int x, int y, int color) {
	//don't attempt writing a pixel outside of screen bounds
	if (x >= screen->window->size.width || y >= screen->window->size.height) return;

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
	for (int loc = 0; loc < (screen->window->size.width * screen->window->size.height * (screen->depth / 8)); loc += (screen->depth / 8)) {
		memcpy(&screen->vmem[loc], color.val, (screen->depth / 8) * sizeof(uint8_t));
	}
}

void write_screen(Screen* screen) {
	vsync();
	memcpy((char*)screen->physbase, screen->vmem, (screen->window->size.width * screen->window->size.height * (screen->depth / 8)));
}

void rainbow_animation(Screen* screen, Rect r) {
	//ROY G BIV
	//int colors[] = {0xFF0000, 0xFF7000, 0xFFFF00, 0x00FF00, 0x0000FF, 0x4B0082, 0x9400D3};
	int colors[] = {4, 42, 44, 46, 1, 13, 34};
	for (int i = 0; i < 7; i++) {
		Coordinate origin = create_coordinate(r.origin.x + (r.size.width / 7) * i, r.origin.y);
		Size size = create_size((r.size.width / 7), r.size.height);
		Rect seg = create_rect(origin, size);

		draw_rect(screen, seg, colors[i], THICKNESS_FILLED);
		sleep(500 / 7);
	}
}

void vga_boot_screen(Screen* screen) {
	Color color;
	color.val[0] = 0;
	fill_screen(screen, color);

	Coordinate p1 = create_coordinate(screen->window->size.width / 2, screen->window->size.height * 0.25);
	Coordinate p2 = create_coordinate(screen->window->size.width / 2 - 25, screen->window->size.height * 0.25 + 50);
	Coordinate p3 = create_coordinate(screen->window->size.width / 2 + 25, screen->window->size.height * 0.25 + 50);
	Triangle triangle = create_triangle(p1, p2, p3);
	draw_triangle(screen, triangle, 2, 5);

	Font* font_map = setup_font();
	draw_string(screen, font_map, "axle os", screen->window->size.width / 2 - 35, screen->window->size.height * 0.6, 2);

	float rect_length = screen->window->size.width / 3;
	Coordinate origin = create_coordinate((screen->window->size.width/2) - (rect_length / 2), screen->window->size.height / 4 * 3);
	Size sz = create_size(rect_length - 5, screen->window->size.height / 16);
	Rect border_rect = create_rect(origin, sz);

	//fill the rectangle with white initially
	draw_rect(screen, border_rect, 15, 1);

	sleep(500);

	Coordinate rainbow_origin = create_coordinate(origin.x + 2, origin.y + 2);
	Size rainbow_size = create_size(rect_length - 4, sz.height - 3);
	Rect rainbow_rect = create_rect(rainbow_origin, rainbow_size);
	rainbow_animation(screen, rainbow_rect);    

	sleep(250);
}
