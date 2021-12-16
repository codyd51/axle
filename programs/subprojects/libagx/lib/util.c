#include "util.h"

Window* containing_window(View* v) {
	//find root window
	View* view = v;
	while (view->superview) {
		view = view->superview;
	}

	Screen* screen = gfx_screen();
	if (screen->window->title_view == view || screen->window->content_view == view) {
		return screen->window;
	}

	//traverse view hierarchy, find window which has view as its title or content view
	for (int32_t i = 0; i < screen->window->subviews->size; i++) {
		Window* window = (Window*)array_m_lookup(screen->window->subviews, i);

		//if user passed a Window, check against that
		if (window == (Window*)view) return window;

		if (window->title_view == view || window->content_view == view) return window;
		for (int32_t j = 0; j < window->subviews->size; j++) {
			Window* subwindow = (Window*)array_m_lookup(window->subviews, j);
			if (subwindow->title_view == view || subwindow->content_view == view) return subwindow;
		}
	}
	return screen->window;
}

Rect absolute_frame(View* view) {
	Rect ret = view->frame;
	//find root view
	View* v = view;
	while (v->superview) {
		v = v->superview;
		ret = convert_frame(v, ret);
	}

	//find containing window
	Window* win = containing_window(v);
	ASSERT(win, "couldn't find container window!");

	return convert_rect(win->frame, ret);
}
