#include "gfx.h"
#include "window.h"
#include "view.h"
#include "button.h"
#include "util.h"
#include <std/math.h>
#include <std/std.h>
#include <user/xserv/animator.h>

#define MAX_ELEMENTS 64

static void close_button_clicked(Button* b) {
	Window* w = containing_window(b->superview);
	kill_window(w);
}

static void minimize_button_clicked(Button* b) {
	close_button_clicked(b);
}

static View* create_title_view(Window* window) {
	if (!window) return NULL;

	Rect title_view_frame = rect_make(point_make(0, 0), size_make(window->frame.size.width, WINDOW_TITLE_VIEW_HEIGHT));
	View* title_view = create_view(title_view_frame);
	title_view->background_color = window->border_color;

	Button* close_button = create_button(rect_make(point_zero(), size_make(CHAR_WIDTH * 2, title_view->frame.size.height)), "X");
	close_button->mousedown_handler = (event_handler)&close_button_clicked;
	add_button(title_view, close_button);

	Button* minimize_button = create_button(rect_make(point_make(rect_max_x(close_button->frame), 0), size_make(CHAR_WIDTH * 2, title_view->frame.size.height)), "-");
	minimize_button->mousedown_handler = (event_handler)&minimize_button_clicked;
	add_button(title_view, minimize_button);

	//add title label to title view
	int label_length = MAX((int)strlen(window->title), 16) * CHAR_WIDTH;
	Rect label_frame = rect_make(point_make(rect_max_x(minimize_button->frame) + 15, title_view_frame.size.height / 2 - (CHAR_HEIGHT / 2)), size_make(label_length, CHAR_HEIGHT));
	Label* title_label = create_label(label_frame, window->title);
	title_label->text_color = color_black();
	add_sublabel(title_view, title_label);

	Bmp* dots = create_bmp(title_view_frame, create_layer(title_view_frame.size));
	uint8_t* ref = dots->layer->raw;
	for (int y = 0; y < dots->frame.size.height; y++) {
		for (int x = 0; x < dots->frame.size.width; x++) {
			if (!((x + y) % 2)) {
				*ref++ = 50;
				*ref++ = 50;
				*ref++ = 50;
			}
			else {
				*ref++ = 200;
				*ref++ = 160;
				*ref++ = 90;
			}
		}
	}
	add_bmp(title_view, dots);

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
	window->border_color = color_make(50, 122, 40);
	window->border_width = 1;
	window->subviews = array_m_create(MAX_ELEMENTS);
	window->title = "Window";
	window->animations = array_m_create(8);

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

	int idx = array_m_index(window->subviews, subwindow);
	if (idx != ARR_NOT_FOUND) {
		array_m_remove(window->subviews, idx);
	}
	subwindow->superview = NULL;
	mark_needs_redraw((View*)window);
}

void present_window(Window* window) {
	window->layer->alpha = 0.0;

	Screen* current = gfx_screen();
	add_subwindow(current->window, window);

	float to = 1.0;
	ca_animation* anim = create_animation(ALPHA_ANIM, &to, 0.25);
	add_animation(window, anim);
}

void kill_window(Window* window) {
	remove_subwindow(gfx_screen()->window, window);

	if (window->teardown_handler) {
		event_handler teardown = window->teardown_handler;
		teardown(window, NULL);
	}

	//window_teardown(window);
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

bool window_presented(Window* w) {
	Screen* s = gfx_screen();
	return (array_m_index(s->window->subviews, w) != ARR_NOT_FOUND);
}

