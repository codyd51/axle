#include "view.h"
#include <std/kheap.h>
#include <gfx/lib/shapes.h>

View* create_view(Rect frame) {
	View* view = kmalloc(sizeof(View));
	view->frame = frame;
	view->background_color = 0x00FF00;
	view->subviews = create_mutable_array(16);
	return view;
}

Window* create_window(Rect frame) {
	Window* window = kmalloc(sizeof(Window));
	window->size = frame.size;
	window->frame = frame;
	window->border_color = 0x0000FF;
	window->subwindows = create_mutable_array(16);
		
	return window;
}

void add_subview(View* view, View* subview) {
	insert_mutable_array(subview, &(view->subviews));
	//subview->superview = view;
}

void add_subwindow(Window* window, Window* subwindow) {
	insert_mutable_array(subwindow, &(window->subwindows));
}

