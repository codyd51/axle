#include "window.h"
#include "view.h"
#include <std/std.h>

#define MAX_ELEMENTS 64

static View* create_title_view(Window* window) {
	if (!window) return NULL;

	Rect title_view_frame = rect_make(point_make(0, 0), size_make(window->frame.size.width, 20));
	View* title_view = create_view(title_view_frame);
	title_view->background_color = window->border_color;

	//add title label to title view
	Rect label_frame = rect_make(point_make(15, 5), title_view_frame.size);
	Label* title_label = create_label(label_frame, window->title);
	title_label->text_color = color_black();
	add_sublabel(title_view, title_label);

	/*
	//add close button
	int close_rad = 3;
	Bmp* close_button = load_bmp(rect_make(point_make(close_rad * 2, label_frame.origin.y+ close_rad), size_make(25, 25)), "close.bmp");
	add_bmp(title_view, close_button);
	*/
	return title_view;
}

static View* create_content_view(Window* window, bool root) {
	if (!window) return NULL;

	int title_height = 20;
	if (window->title_view) {
		title_height = window->title_view->frame.size.height;
	} if (root) title_height = 0;

	Rect inner_frame = rect_make(point_make((window->border_width * 2), title_height), size_make(window->frame.size.width - (window->border_width * 4), window->frame.size.height - title_height - (window->border_width * 2)));
	View* content_view = create_view(inner_frame);
	content_view->background_color = color_make(255, 255, 255);

	return content_view;
}

Window* create_window_int(Rect frame, bool root) {
	Window* window = (Window*)kmalloc(sizeof(Window));
	memset(window, 0, sizeof(Window));

	window->layer = create_layer(frame.size);
	window->size = frame.size;
	window->frame = frame;
	window->border_color = color_make(120, 245, 80);
	window->border_width = 1;
	window->subviews = array_m_create(MAX_ELEMENTS);
	window->title = "Window";

	//root window doesn't have a title view
	if (!root) {
		window->title_view = create_title_view(window);
	}
	window->content_view = create_content_view(window, root);

	window->needs_redraw = 1;

	return window;
}

Window* create_window(Rect frame) {
	return create_window_int(frame, false);
}

void add_subwindow(Window* window, Window* subwindow) {
	if (!window || !subwindow) return;

	array_m_insert(window->subviews, subwindow);
	subwindow->superview = window;
	mark_needs_redraw((View*)window);
}

void remove_subwindow(Window* window, Window* subwindow) {
	if (!window || !subwindow) return;

	array_m_remove(window->subviews, array_m_index(window->subviews, subwindow));
	subwindow->superview = NULL;
	mark_needs_redraw((View*)window);
}

void set_border_width(Window* window, int width) {
	if (!window) return;

	window->border_width = width;
	mark_needs_redraw((View*)window);
}

void window_teardown(Window* window) {
	if (!window) return;

	for (int i = 0; i < window->subviews->size; i++) {
		Window* window = (Window*)array_m_lookup(window->subviews, i);
		window_teardown(window);
	}
	//free subviews array
	array_m_destroy(window->subviews);

	//free the views associated with this window
	view_teardown(window->title_view);
	view_teardown(window->content_view);

	//free backing layer
	layer_teardown(window->layer);

	//finally, free window itself
	kfree(window);
}
