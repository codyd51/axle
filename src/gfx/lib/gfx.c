#include "gfx.h"
#include <std/std.h>
#include <kernel/kernel.h>
#include <std/timer.h>
#include <gfx/font/font.h>
#include "shapes.h"
#include <std/std.h>
#include <std/kheap.h>
#include <tests/gfx_test.h>
#include <kernel/drivers/vga/vga.h>
#include <kernel/drivers/vesa/vesa.h>
#include "color.h"
#include "shader.h"

//private Window function to create root window
Window* create_window_int(Rect frame, bool root);

static int current_depth = 0;
void process_gfx_switch(int new_depth) {
	current_depth = new_depth;
}

int gfx_depth() {
	if (!current_depth) {
		//fall back on assuming VESA
		current_depth = VESA_DEPTH;
	}
	return current_depth;
}

int gfx_bpp() {
	if (!current_depth) {
		//fall back on assuming VESA
		current_depth = VESA_DEPTH;
	}
	//each px component is 8 bits
	return current_depth / 8;
}

Vec2d vec2d(double x, float y) {
	Vec2d vec;
	vec.x = x;
	vec.y = y;
	return vec;
}

Screen* screen_create(Size dimensions, uint32_t* physbase, uint8_t depth) {
			Screen* screen = kmalloc(sizeof(Screen));

			//linear frame buffer (LFB) address
			screen->physbase = physbase;
			screen->window = create_window_int(rect_make(point_make(0, 0), dimensions), true);
			screen->window->superview = NULL;
			screen->depth = depth;
			//8 bits in a byte
			screen->bpp = depth / 8;
			screen->vmem = create_layer(dimensions);

			return screen;
}

void gfx_teardown(Screen* screen) {
	if (!screen) return;

	//free screen
	window_teardown(screen->window);
	kfree(screen->vmem);
	kfree(screen);
}

void switch_to_text() {
	regs16_t regs;
	regs.ax = 0x0003;
	int32(0x10, &regs);
	term_scroll(TERM_SCROLL_UP);
}

void vsync() {
	//wait until previous retrace has ended
	do {} while (inb(0x3DA) & 8);

	//wait until new trace has just begun
	do {} while (!(inb(0x3DA) & 8));
}

void fill_screen(Screen* screen, Color color) {
	memset(screen->window->layer->raw, color.val[0], screen->window->size.width * screen->window->size.height * gfx_bpp());
}

void write_screen(Screen* screen) {
	vsync();
	memcpy(screen->physbase, screen->vmem->raw, screen->vmem->size.width * screen->vmem->size.height * gfx_bpp());
}

void rainbow_animation(Screen* screen, Rect r, int animationStep) {
	//ROY G BIV
	int colors[] = {4, 42, 44, 46, 1, 13, 34};
	for (int i = 0; i < 7; i++) {
		Coordinate origin = point_make(r.origin.x + (r.size.width / 7) * i, r.origin.y);
		Size size = size_make((r.size.width / 7), r.size.height);
		Rect seg = rect_make(origin, size);

		Color col;
		col.val[0] = colors[i];
		draw_rect(screen->vmem, seg, col, THICKNESS_FILLED);
		write_screen(screen);
		
		sleep(animationStep / 7);
	}
}

void vga_boot_screen(Screen* screen) {
	Color color;
	color.val[0] = 0;
	fill_screen(screen, color);

	Coordinate p1 = point_make(screen->window->size.width / 2, screen->window->size.height * 0.25);
	Coordinate p2 = point_make(screen->window->size.width / 2 - 25, screen->window->size.height * 0.25 + 50);
	Coordinate p3 = point_make(screen->window->size.width / 2 + 25, screen->window->size.height * 0.25 + 50);
	Triangle triangle = triangle_make(p1, p2, p3);
	Color tri_col;
	tri_col.val[0] = 2;
	draw_triangle(screen->vmem, triangle, tri_col, 5);

	Coordinate lab_origin = point_make(screen->window->size.width / 2 - (3.75 * 8), screen->window->size.height * 0.625);
	Size lab_size = size_make((10 * strlen("axle os")), 12);
	Label* label = create_label(rect_make(lab_origin, lab_size), "axle os");
	label->text_color = color_make(2, 0, 0);
	add_sublabel(screen->window->content_view, label);

	extern void draw_label(ca_layer* dest, Label* label);
	draw_label(screen->vmem, label);

	float rect_length = screen->window->size.width / 3;
	Coordinate origin = point_make((screen->window->size.width/2) - (rect_length / 2), screen->window->size.height / 4 * 3);
	Size sz = size_make(rect_length - 5, screen->window->size.height / 16);
	Rect border_rect = rect_make(origin, sz);

	//fill the rectangle with white initially
	Color white;
	white.val[0] = 15;
	draw_rect(screen->vmem, border_rect, white, 1);
	
	write_screen(screen);

	sleep(500);

	Coordinate rainbow_origin = point_make(origin.x + 2, origin.y + 2);
	Size rainbow_size = size_make(rect_length - 4, sz.height - 3);
	Rect rainbow_rect = rect_make(rainbow_origin, rainbow_size);
	rainbow_animation(screen, rainbow_rect, 750);

	sleep(250);
}
