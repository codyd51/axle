#include "gfx_test.h"
#include <std/math.h>
#include <user/shell/shell.h>
#include <gfx/lib/color.h>
#include <gfx/lib/shapes.h>
#include <kernel/drivers/vga/vga.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <user/programs/calculator.h>
#include <user/xserv/animator.h>

//draw Mandelbrot set
void draw_mandelbrot(Screen* screen, bool rgb) {
	//each iteration, we calculate: new = old * old + p, where p is current pixel,
	//old starts at the origin
	double pr, pi; //real and imaginary parts of pixel p
	double new_re, new_im, old_re, old_im; //real & imaginary parts of new and old z
	double zoom = 1, move_x = -0.5, move_y = 0;
	int max_iterations = 300;

	//for every pixel
	for (int y = 0; y < screen->window->frame.size.height; y++) {
		for (int x = 0; x < screen->window->frame.size.width; x++) {
			//calculate real and imaginary part of z
			//based on pixel location and zoom and position vals
			pr = 1.5 * (x - screen->window->frame.size.width / 2) / (0.5 * zoom * screen->window->frame.size.width) + move_x;
			pi = (y - screen->window->frame.size.height / 2) / (0.5 * zoom * screen->window->frame.size.height) + move_y;
			new_re = new_im = old_re = old_im = 0; //start at 0.0 int i;
			int i;
			for (i = 0; i < max_iterations; i++) {
				//remember value of previous iteration
				old_re = new_re;
				old_im = new_im;

				//actual iteration, real and imaginary parts are calculated
				new_re = old_re * old_re - old_im * old_im + pr;
				new_im = 2 * old_re * old_im + pi;

				//if point is outside circle with radius 2, stop
				if ((new_re * new_re + new_im * new_im) > 4) break;
			}
			Color color = color_make(5 + i % max_iterations, 0, 0);
			if (rgb) {
				color = color_make(i, i % max_iterations, (i < max_iterations) ? 0 : 255);
			}
			putpixel(screen->vmem, x, y, color);
		}
	}
}

void draw_burning_ship(Screen* screen, bool rgb) {
	//each iteration, we calculate: new = old * old + p, where p is current pixel,
	//old starts at the origin
	double pr, pi; //real and imaginary parts of pixel p
	double new_re, new_im, old_re, old_im; //real & imaginary parts of new and old z
	double zoom = 1, move_x = -0.5, move_y = 0;
	int max_iterations = 300;

	//for every pixel
	for (int y = 0; y < screen->window->size.height; y++) {
		for (int x = 0; x < screen->window->size.width; x++) {
			//calculate real and imaginary part of z
			//based on pixel location and zoom and position vals
			pr = 1.5 * (x - screen->window->size.width / 2) / (0.5 * zoom * screen->window->size.width) + move_x;
			pi = (y - screen->window->size.height / 2) / (0.5 * zoom * screen->window->size.height) + move_y;
			new_re = new_im = old_re = old_im = 0; //start at 0.0

			int i;
			for (i = 0; i < max_iterations; i++) {
				//remember value of previous iteration
				old_re = new_re;
				old_im = new_im;

				//actual iteration, real and imaginary parts are calculated
				if (old_im < 0) old_im *= -1;
				if (old_re < 0) old_re *= -1;
				new_re = (old_re * old_re) - (old_im * old_im) + pr;
				new_im = 2 * old_re * old_im + pi;

				//if point is outside circle with radius 2, stop
				if ((new_re * new_re + new_im * new_im) > 4) break;
			}
			Color color = color_make(5 + i % max_iterations, 0, 0);
			if (rgb) {
				color = color_make(i, i % max_iterations, (i < max_iterations) ? 0 : 255);
			}
			putpixel(screen->vmem, x, y, color);
		}
	}
}

//draw julia set
void draw_julia(Screen* screen, bool rgb) {
	//each iteration, we calculate: new = old * old + c
	//c is constant
	//old starts at current pixel
	double cRe, cIm; //real and imaginary parts of c
	double new_re, new_im, old_re, old_im; //real and imaginary parts of new and old
	double zoom = 1, move_x = 0, move_y = 0;
	int max_iterations = 300;

	//pick some values for constant c
	//determines shape
	//cRe = -0.7;
	cRe = -0.7;
	//cIm = 0.27015;
	cIm = -0.61841;

	int w = screen->window->size.width;
	int h = screen->window->size.height;

	//for every pixel
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			//calculate real and imaginary part of z
			//based on pixel location and zoom and position values
			new_re = 1.5 * (x - w / 2) / (0.5 * zoom * w) + move_x;
			new_im = (y - h / 2) / (0.5 * zoom * h) + move_y;

			int i;
			for (i = 0; i < max_iterations; i++) {
				//remember value of previous iteration
				old_re = new_re;
				old_im = new_im;

				//the actual iteration, real and imaginary part are calculated
				new_re = old_re * old_re - old_im * old_im + cRe;
				new_im = 2 * old_re * old_im + cIm;

				//if point is outside the circle with radius 2: stop
				if ((new_re * new_re + new_im * new_im) > 4) break;
			}

			//Color color = color_make(180 * (i % max_iterations), 20 * (i % max_iterations), 100 * (i % max_iterations));
			Color color = color_make(i % max_iterations + 2, 0, 0);
			if (rgb) {
				color = color_make(i, i % max_iterations, (i < max_iterations) ? 0 : 255);
			}
			putpixel(screen->vmem, x, y, color);
		}
	}
}

void test_triangles(Screen* screen) {
	fill_screen(screen, color_make(0, 0, 0));

	Coordinate p1 = point_make(screen->window->size.width / 2, 0);
	Coordinate p2 = point_make(0, screen->window->size.height - 10);
	Coordinate p3 = point_make(screen->window->size.width, screen->window->size.height - 10);

	for (int i = 1; i <= 12; i++) {
		Triangle t = triangle_make(p1, p2, p3);
		draw_triangle(screen->vmem, t, color_make(i, 0, 0), THICKNESS_FILLED);

		p1.y += i * 2;
		p2.x += i * 1.5;
		p2.y -= i / 2;
		p3.x -= i * 1.5;
		p3.y -= i / 2;
	}
}

void test_rects(Screen* screen) {
	fill_screen(screen, color_make(0, 0, 0));

	Coordinate origin = point_make(0, 0);
	Size sz = screen->window->size;

	for (int i = 0; i < 20; i++) {
		Rect rt = rect_make(origin, sz);
		draw_rect(screen->vmem, rt, color_make(i, 0, 0), 1);

		origin.x += 4;
		origin.y += 4;
		sz.width -= 8;
		sz.height -= 8;
	}
}

void test_circles(Screen* screen) {
	fill_screen(screen, color_make(0, 0, 0));

	Coordinate center = point_make(screen->window->size.width/2, screen->window->size.height/2);
	int radius = screen->window->size.height/2;

	for (int i = 0; i < 26; i++) {
		Circle c = circle_make(center, radius);
		draw_circle(screen->vmem, c, color_make(i, 0, 0), 1);

		radius -= 4;
	}
}

void test_lines(Screen* screen) {
	fill_screen(screen, color_make(0, 0, 0));

	ca_layer* layer = screen->vmem;
	for (int i = 0; i < 128; i++) {
		int p1x = rand() % (layer->size.width + 1);
		int p1y = rand() % (layer->size.height + 1);
		int p2x = rand() % (layer->size.width + 1);
		int p2y = rand() % (layer->size.height + 1);

		Coordinate p1 = point_make(p1x, p1y);
		Coordinate p2 = point_make(p2x, p2y);
		Line line = line_make(p1, p2);
		draw_line(layer, line, color_make(i, 0, 0), 1);
	}
}

extern void draw_label(ca_layer*, Label*);
void test_text(Screen* screen) {
	fill_screen(screen, color_make(0, 0, 0));

	char* str = "Lorem ipsum dolor sit amet consectetur apipiscing elit Donex purus arcu suscipit ed felis eu blandit blandit quam Donec finibus euismod lobortis Sed massa nunc malesuada ac ante eleifend dictum laoreet massa Aliquam nec dictum turpis pellentesque lacinia ligula Donec et tellus maximum dapibus justo auctor egestas sapien Integer venantis egesta malesdada Maecenas venenatis urna id posuere bibendum eros torto gravida ipsum sed tempor arcy andte ac odio Morbi elementum libero id velit bibendum auctor It sit amet ex eget urna venenatis laoreet Proin posuere urna nec ante tutum lobortis Cras nec elit tristique dolor congue eleifend";
	Label* label = create_label(rect_make(point_make(0, 0), size_make(screen->window->size.width, screen->window->size.height)), str);
	label->text_color = color_make(12, 0, 0);
	add_sublabel(screen->window->content_view, label);
	draw_label(screen->vmem, label);

	kfree(label);
}

void draw_test_button(Screen* screen) {
	fill_screen(screen, color_make(0, 0, 0));

	Coordinate origin = point_make(screen->window->size.width * 0.25, screen->window->size.height * 0.25);
	Size sz = size_make(screen->window->size.width * 0.25, screen->window->size.height * 0.25);
	Rect r = rect_make(origin, sz);
	draw_rect(screen->vmem, r, color_make(2, 0, 0), 1);

	Coordinate in_origin = point_make(origin.x + 1, origin.y + 1);
	Size in_size = size_make(sz.width - 2, sz.height - 2);
	Rect in_rect = rect_make(in_origin, in_size);
	draw_rect(screen->vmem, in_rect, color_make(12, 0, 0), 30);

	Coordinate p1 = point_make(origin.x + sz.width * 0.1, origin.y + sz.height * 0.1);
	Coordinate p2 = point_make(origin.x + sz.width * 0.1, origin.y + sz.height * 0.9);
	Coordinate p3 = point_make(origin.x + sz.width * 0.4, origin.y + sz.height * 0.5);
	Triangle tri = triangle_make(p1, p2, p3);
	draw_triangle(screen->vmem, tri, color_make(15, 0, 0), 1);

	Rect label_rect = rect_make(point_make(p3.x + 5, in_origin.y), size_make(in_rect.size.width, in_rect.size.height));
	Label* play_label = create_label(label_rect, "Play");
	play_label->text_color = color_make(1, 0, 0);
	add_sublabel(screen->window->content_view, play_label);
	draw_label(screen->vmem, play_label);

	kfree(play_label);
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
void test_gfx(int argc, char **argv) {
	if (fork("gfxtest")) {
		//parent
		return;
	}

	int delay = 1000;

	Screen* screen = switch_to_vga();

	fill_screen(screen, color_make(0, 0, 0));
	draw_test_button(screen);
	write_screen(screen);
	sleep(delay);

	fill_screen(screen, color_make(0, 0, 0));
	test_lines(screen);
	write_screen(screen);
	sleep(delay);

	fill_screen(screen, color_make(0, 0, 0));
	test_circles(screen);
	write_screen(screen);
	sleep(delay);

	fill_screen(screen, color_make(0, 0, 0));
	test_rects(screen);
	write_screen(screen);
	sleep(delay);

	fill_screen(screen, color_make(0, 0, 0));
	test_triangles(screen);
	write_screen(screen);
	sleep(delay);

	fill_screen(screen, color_make(0, 0, 0));
	test_text(screen);
	write_screen(screen);
	sleep(delay);

	fill_screen(screen, color_make(0, 0, 0));
	draw_julia(screen, false);
	write_screen(screen);
	sleep(delay);

	fill_screen(screen, color_make(0, 0, 0));
	draw_mandelbrot(screen, false);
	write_screen(screen);
	sleep(delay);

	fill_screen(screen, color_make(0, 0, 0));
	draw_burning_ship(screen, false);
	write_screen(screen);
	sleep(delay);

	gfx_teardown(screen);
	switch_to_text();

	_kill();
}
#pragma GCC diagnostic pop


void test_xserv() {
	Window* alpha_win = create_window(rect_make(point_make(100, 200), size_make(250, 250)));
	alpha_win->title = "Alpha animation";
	alpha_win->content_view->background_color = color_green();
	Label* bye_label = create_label(rect_make(point_make(50, 75), size_make(125, CHAR_HEIGHT)), "Goodbye!");
	add_sublabel(alpha_win->content_view, bye_label);
	float to = 0.0;
	ca_animation* anim = create_animation(ALPHA_ANIM, &to, 2.0);
	add_animation(alpha_win, anim);
	present_window(alpha_win);
	
	Window* color_win = create_window(rect_make(point_make(700, 200), size_make(250, 250)));
	color_win->title = "Color animation";
	color_win->content_view->background_color = color_green();
	Label* color_label = create_label(rect_make(point_make(50, 75), size_make(150, CHAR_HEIGHT)), "I'm feeling blue...");
	add_sublabel(color_win->content_view, color_label);
	Color color_to = color_blue();
	ca_animation* color = create_animation(COLOR_ANIM, &color_to, 2.0);
	add_animation(color_win, color);
	present_window(color_win);

	Window* pos_win = create_window(rect_make(point_make(400, 200), size_make(250, 250)));
	pos_win->title = "Position animation";
	Coordinate pos_to = alpha_win->frame.origin;
	ca_animation* pos = create_animation(POS_ANIM, &pos_to, 2.0);
	Label* label = create_label(rect_make(point_make(75, 125), size_make(125, CHAR_HEIGHT)), "Wheeee!");
	add_sublabel(pos_win->content_view, label);
	add_animation(pos_win, pos);
	present_window(pos_win);
}

