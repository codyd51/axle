#include "gfx_test.h"
#include <gfx/lib/shapes.h>
#include <gfx/font/font.h>
#include <user/shell/shell.h>

//draw Mandelbrot set
void draw_mandelbrot(Screen* screen) {
	//each iteration, we calculate: new = old * old + p, where p is current pixel,
	//old starts at the origin
	double pr, pi; //real and imaginary parts of pixel p
	double new_re, new_im, old_re, old_im; //real & imaginary parts of new and old z
	double zoom = 1, move_x = -0.5, move_y = 0; 
	int max_iterations = 300;

	//for every pixel
	for (int y = 0; y < screen->window.size.height; y++) {
		for (int x = 0; x < screen->window.size.width; x++) {
			//calculate real and imaginary part of z
			//based on pixel location and zoom and position vals
			pr = 1.5 * (x - screen->window.size.width / 2) / (0.5 * zoom * screen->window.size.width) + move_x;
			pi = (y - screen->window.size.height / 2) / (0.5 * zoom * screen->window.size.height) + move_y;
			new_re = new_im = old_re = old_im = 0; //start at 0.0

			int i;
			for (i = 0; i < max_iterations; i++) {
				//remember value of previous iteration
				old_re = new_re;
				old_im = new_im;

				//actual iteration, real and imaginary parts are calculated
				new_re = old_re * old_re - old_im * old_im + pr;
				new_im = 2 * old_re * old_im + pi;

				//if piont is outside circle with radius 2, stop
				if ((new_re * new_re + new_im * new_im) > 4) break;
			}
			int color = i % max_iterations;
			putpixel(screen, x, y, color);
		}
	}
}

//draw julia set
void draw_julia(Screen* screen) {
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

	int w = screen->window.size.width;
	int h = screen->window.size.height;

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

			int color = (i % max_iterations);
			putpixel(screen, x, y, 0x0000 + (color * 0xFF0099));
		}
	}
}

void test_triangles(Screen* screen) {
	fill_screen(screen, 0);

	Coordinate p1 = create_coordinate(screen->window.size.width / 2, 0);
	Coordinate p2 = create_coordinate(0, screen->window.size.height - 10);
	Coordinate p3 = create_coordinate(screen->window.size.width, screen->window.size.height - 10);

	for (int i = 1; i <= 12; i++) {
		Triangle t = create_triangle(p1, p2, p3);
		draw_triangle(screen, t, i, 1);

		p1.y += i * 2;
		p2.x += i * 1.5;
		p2.y -= i / 2;
		p3.x -= i * 1.5;
		p3.y -= i / 2;
	}
}

void test_rects(Screen* screen) {
	fill_screen(screen, 0);

	Coordinate origin = create_coordinate(0, 0);
	Size sz = screen->window.size;
	
	for (int i = 0; i < 20; i++) {
		Rect rt = create_rect(origin, sz);
		draw_rect(screen, rt, i, 1);

		origin.x += 4;
		origin.y += 4;
		sz.width -= 8;
		sz.height -= 8;
	}
}

void test_circles(Screen* screen) {
	fill_screen(screen, 0);

	Coordinate center = create_coordinate(screen->window.size.width/2, screen->window.size.height/2);
	int radius = screen->window.size.height/2;

	for (int i = 0; i < 26; i++) {
		Circle c = create_circle(center, radius);
		draw_circle(screen, c, i, 1);

		radius -= 4;
	}
}

void test_lines(Screen* screen) {
	fill_screen(screen, 0);

	for (int i = 0; i < 128; i++) {
		int p1x = rand() % (screen->window.size.width + 1);
		int p1y = rand() % (screen->window.size.height + 1);
		int p2x = rand() % (screen->window.size.width + 1);
		int p2y = rand() % (screen->window.size.height + 1);

		Coordinate p1 = create_coordinate(p1x, p1y);
		Coordinate p2 = create_coordinate(p2x, p2y);
		Line line = create_line(p1, p2);
		draw_line(screen, line, i, 1);
	}
}

void test_text(Screen* screen) {
	fill_screen(screen, 0);
	Font* font = setup_font();

	char* str = "Lorem ipsum dolor sit amet consectetur apipiscing elit Donex purus arcu suscipit ed felis eu blandit blandit quam Donec finibus euismod lobortis Sed massa nunc malesuada ac ante eleifend dictum laoreet massa Aliquam nec dictum turpis pellentesque lacinia ligula Donec et tellus maximum dapibus justo auctor egestas sapien Integer venantis egesta malesdada Maecenas venenatis urna id posuere bibendum eros torto gravida ipsum sed tempor arcy andte ac odio Morbi elementum libero id velit bibendum auctor It sit amet ex eget urna venenatis laoreet Proin posuere urna nec ante tutum lobortis Cras nec elit tristique dolor congue eleifend";

	draw_string(screen, font, str, 0, 0, 12);
}

void draw_button(Screen* screen) {
	fill_screen(screen, 0);

	Coordinate origin = create_coordinate(screen->window.size.width * 0.25, screen->window.size.height * 0.25);
	Size sz = create_size(screen->window.size.width * 0.25, screen->window.size.height * 0.25);
	Rect r = create_rect(origin, sz);
	draw_rect(screen, r, 2, 1);

	Coordinate in_origin = create_coordinate(origin.x + 1, origin.y + 1);
	Size in_size = create_size(sz.width - 2, sz.height - 2);
	Rect in_rect = create_rect(in_origin, in_size);
	draw_rect(screen, in_rect, 12, 30);

	Coordinate p1 = create_coordinate(origin.x + sz.width * 0.1, origin.y + sz.height * 0.1);
	Coordinate p2 = create_coordinate(origin.x + sz.width * 0.1, origin.y + sz.height * 0.9);
	Coordinate p3 = create_coordinate(origin.x + sz.width * 0.4, origin.y + sz.height * 0.5);
	Triangle tri = create_triangle(p1, p2, p3);
	draw_triangle(screen, tri, 15, 1);

	Font* font = setup_font();
	draw_string(screen, font, "Play", p3.x + 5, p3.y - 4, 3);
}

void test_gfx(int argc, char **argv) {
	Screen* screen = switch_to_vga();

	fill_screen(screen, 0);

	draw_button(screen);
	sleep(2000);

	test_lines(screen);
	sleep(2000);

	test_circles(screen);
	sleep(2000);

	test_rects(screen);
	sleep(2000);

	test_triangles(screen);
	sleep(2000);

	test_text(screen);
	sleep(2000);

	draw_julia(screen);
	sleep(2000);

	draw_mandelbrot(screen);
	sleep(2000);

	switch_to_text(screen);
}
