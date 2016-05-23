#include "xserv.h"
/*
void traverse_view(Screen* screen, View view) {
	for (int i = 0; i < screen->window.subviewsCount; i++) {
		traverse_view(screen, view.subviews[i]);
	}
	static int i = 1;
	draw_rect(screen, view.frame, (0xFF0000 * (i * 100)), THICKNESS_FILLED);
	i++;
}
*/
/*
void draw_view(Screen* screen, View view) {
	//just fill in for now
	draw_rect(screen, view.frame, 0x00FF00, THICKNESS_FILLED);
}
*/
void draw_window(Screen* screen, Window* window) {
/*
	//paint navy blue window
	draw_rect(screen, window.frame, 0x0000FF, THICKNESS_FILLED);
	
	//put a small red square in top left corner of the window
	Size close_button_size = create_size(5, 5);
	Rect close_button = create_rect(window.frame.origin, close_button_size);
	draw_rect(screen, close_button, 0xFF0000, THICKNESS_FILLED);

	//put title bar
	//
	
	//paint every child view
	View* view = window.view;
	for (int i = 0; i < view->subviewsCount; i++) {
		draw_view(screen, view->subviews[i]);
	}
*/
	//paint blue window
	printf("\nWindow frame: {{%d,%d},{%d,%d}}", window->frame.origin.x, window->frame.origin.y, window->frame.size.width, window->frame.size.height);
	draw_rect(screen, window->frame, 0x0000FF, THICKNESS_FILLED);
}

void draw_desktop(Screen* screen) {
	Coordinate origin = create_coordinate(0, 0);
	Size sz = create_size(screen->window->size.width, screen->window->size.height);
	Rect r = create_rect(origin, sz);
	draw_rect(screen, r, 0xC0C0C0, THICKNESS_FILLED);

	//paint every child window
	for (int i = 0; i < screen->window->subwindows.size; i++) {
		Window* win = (Window*)(lookup_mutable_array(i, &(screen->window->subwindows)));
		draw_window(screen, win);
	}

	Coordinate p1 = create_coordinate(50, 0);
	Coordinate p2 = create_coordinate(25, 50);
	Coordinate p3 = create_coordinate(75, 50);
	Triangle tri = create_triangle(p1, p2, p3);
	draw_triangle(screen, tri, 0xFF0000, 5);
}
void xserv_draw(Screen* screen) {
	draw_desktop(screen);
}
