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
#include "color.h"

void image_teardown(Image* image) {
	kfree(image->bitmap);
	kfree(image);
}

void label_teardown(Label* label) {
	kfree(label);
}

void view_teardown(View* view) {
	for (int i = 0; i < view->subviews.size; i++) {
		View* view = (View*)array_m_lookup(i, &(view->subviews));
		view_teardown(view);
	}
	//free subviews array
	array_m_destroy(&(view->subviews));

	for (int i = 0; i < view->labels.size; i++) {
		Label* label = (Label*)array_m_lookup(i, &(view->labels));
		label_teardown(label);
	}
	//free sublabels
	array_m_destroy(&(view->labels));

	for (int i = 0; i < view->images.size; i++) {
		Image* image = (Image*)array_m_lookup(i, &(view->images));
		image_teardown(image);
	}
	//free subimages
	array_m_destroy(&(view->images));
	
	//finally, free view itself
	kfree(view);
}

void window_teardown(Window* window) {
	for (int i = 0; i < window->subwindows.size; i++) {
		Window* window = (Window*)array_m_lookup(i, &(window->subwindows));
		window_teardown(window);
	}
	//free subwindows array
	array_m_destroy(&(window->subwindows));

	//free the views associated with this window
	view_teardown(window->title_view);
	view_teardown(window->content_view);

	//finally, free window itself
	kfree(window);
}

void gfx_teardown(Screen* screen) {
	//stop refresh loop for this screen
	remove_callback(screen->callback);

	//free screen
	kfree(screen->vmem);
	kfree(screen->font);
	window_teardown(screen->window);
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

void putpixel(Screen* screen, int x, int y, Color color) {
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

void fill_screen(Screen* screen, Color color) {
	for (int loc = 0; loc < (screen->window->size.width * screen->window->size.height * (screen->depth / 8)); loc += (screen->depth / 8)) {
		memcpy(&screen->vmem[loc], color.val[0], (screen->depth / 8) * sizeof(uint8_t));
	}
}

void write_screen(Screen* screen) {
	vsync();
	memcpy((char*)screen->physbase, screen->vmem, (screen->window->size.width * screen->window->size.height * (screen->depth / 8)));
}

void rainbow_animation(Screen* screen, Rect r) {
	//ROY G BIV
	int colors[] = {4, 42, 44, 46, 1, 13, 34};
	for (int i = 0; i < 7; i++) {
		Coordinate origin = point_make(r.origin.x + (r.size.width / 7) * i, r.origin.y);
		Size size = size_make((r.size.width / 7), r.size.height);
		Rect seg = rect_make(origin, size);

		Color col;
		col.val[0] = colors[i];
		draw_rect(screen, seg, col, THICKNESS_FILLED);
		sleep(500 / 7);
	}
}

void vga_boot_screen(Screen* screen) {
	//Color color = color_make(0, 0, 0);
	Color color;
	color.val[0] = 2;
	fill_screen(screen, color);

	Coordinate p1 = point_make(screen->window->size.width / 2, screen->window->size.height * 0.25);
	Coordinate p2 = point_make(screen->window->size.width / 2 - 25, screen->window->size.height * 0.25 + 50);
	Coordinate p3 = point_make(screen->window->size.width / 2 + 25, screen->window->size.height * 0.25 + 50);
	Triangle triangle = triangle_make(p1, p2, p3);
	Color tri_col;
	tri_col.val[0] = 2;
	draw_triangle(screen, triangle, tri_col, 5);

	Coordinate lab_origin = point_make(screen->window->size.width / 2 - 35, screen->window->size.height * 0.6);
	Size lab_size = size_make((10 * strlen("axle os")), 12);
	Label* label = create_label(rect_make(lab_origin, lab_size), "axle os");
	label->text_color = color_make(2, 0, 0);
	draw_label(screen, label);

	float rect_length = screen->window->size.width / 3;
	Coordinate origin = point_make((screen->window->size.width/2) - (rect_length / 2), screen->window->size.height / 4 * 3);
	Size sz = size_make(rect_length - 5, screen->window->size.height / 16);
	Rect border_rect = rect_make(origin, sz);

	//fill the rectangle with white initially
	Color white;
	white.val[0] = 15;
	draw_rect(screen, border_rect, white, 1);

	sleep(500);

	Coordinate rainbow_origin = point_make(origin.x + 2, origin.y + 2);
	Size rainbow_size = size_make(rect_length - 4, sz.height - 3);
	Rect rainbow_rect = rect_make(rainbow_origin, rainbow_size);
	rainbow_animation(screen, rainbow_rect);    
	
	sleep(250);
}
