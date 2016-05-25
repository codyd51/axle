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

	//only draw a title bar if title_view exists
	if (window->title_view) {
		draw_view(screen, window->title_view);
	}

	//put a small red square in top left corner of the window
	Size close_button_size = create_size(5, 5);
	Rect close_button = create_rect(window->frame.origin, close_button_size);
	draw_rect(screen, close_button, 0xFF0000, THICKNESS_FILLED);

	//only draw the content view if content_view exists
	if (window->content_view) {
		draw_view(screen, window->content_view);
	}
}

void add_taskbar(Screen* screen) {
	Size taskbar_size = create_size(screen->window->size.width, screen->window->size.height * 0.05);
	Coordinate taskbar_origin = create_coordinate(0, screen->window->size.height - taskbar_size.height);
	Window* taskbar_window = create_window(create_rect(taskbar_origin, taskbar_size));
	add_subwindow(screen->window, taskbar_window);

	View* taskbar_view = create_view(create_rect(taskbar_origin, taskbar_size));
	taskbar_window->content_view = taskbar_view;
	taskbar_view->background_color = 0x0B889B;

	Coordinate name_label_origin = create_coordinate(taskbar_view->frame.size.width * 0.925, taskbar_view->frame.origin.y + taskbar_view->frame.size.height / 4);
	Rect label_rect = create_rect(name_label_origin, create_size(0, 0));
	Label* name_label = create_label(label_rect, "axle os");
	add_sublabel(taskbar_view, name_label);
}

void draw_desktop(Screen* screen) {
	Coordinate origin = create_coordinate(0, 0);
	Size sz = create_size(screen->window->size.width, screen->window->size.height);
	Rect r = create_rect(origin, sz);
	draw_rect(screen, r, 0xC0C0C0, THICKNESS_FILLED);

	add_taskbar(screen);

	//paint every child window
	for (int i = 0; i < screen->window->subwindows.size; i++) {
		Window* win = (Window*)(lookup_mutable_array(i, &(screen->window->subwindows)));
		draw_window(screen, win);
	}
}

void xserv_draw(Screen* screen) {
	draw_desktop(screen);
}
