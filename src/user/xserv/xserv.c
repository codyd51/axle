#include "xserv.h"

void draw_label(Screen* screen, Label* label) {
	draw_string(screen, screen->font, label->text, label->frame.origin.x, label->frame.origin.y, label->text_color);
}

void draw_view(Screen* screen, View* view) {
	//fill view with its background color
	draw_rect(screen, view->frame, view->background_color, THICKNESS_FILLED);

	//draw any labels this view has
	for (int i = 0; i < view->labels.size; i++) {
		Label* label = (Label*)lookup_mutable_array(i, &(view->labels));
		draw_label(screen, label);
	}

	//draw each subview of this view
	for (int i = 0; i < view->subviews.size; i++) {
		View* subview = (View*)lookup_mutable_array(i, &(view->subviews));
		draw_view(screen, subview);
	}
}

void draw_window(Screen* screen, Window* window) {

	//paint navy blue window
	draw_rect(screen, window->frame, window->border_color, 1);
	
	//put a small red square in top left corner of the window
	Size close_button_size = create_size(5, 5);
	Rect close_button = create_rect(window->frame.origin, close_button_size);
	draw_rect(screen, close_button, 0xFF0000, THICKNESS_FILLED);

	//put title bar
	//
	
	//paint every child view
	View* view = window->view;
	draw_view(screen, view);
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
}

void xserv_draw(Screen* screen) {
	draw_desktop(screen);
}
