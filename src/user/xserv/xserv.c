#include "xserv.h"
#include <stddef.h>
#include <std/math.h>
#include <std/panic.h>

//has the screen been modified this refresh?
static char dirtied = 0;

Window* containing_window_int(Screen* screen, View* v) {
	//find root window
	View* view = v;
	while (view->superview) {
		view = view->superview;
	}

	//traverse view hierarchy, find window which has view as its title or content view
	if (screen->window->title_view == view || screen->window->content_view == view) return screen->window;
	for (unsigned i = 0; i < screen->window->subviews.size; i++) {
		Window* window = array_m_lookup(i, &(screen->window->subviews));
		if (window->title_view == view || window->content_view == view) return window;
		for (unsigned j = 0; j < window->subviews.size; j++) {
			Window* subwindow = array_m_lookup(j, &(window->subviews));
			if (subwindow->title_view == view || subwindow->content_view == view) return subwindow;
		}
	}
	return NULL;
}

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8
#define CHAR_PADDING 2

Rect convert_rect(Rect outer, Rect inner) {
	Rect ret;
	ret.origin.x = inner.origin.x + outer.origin.x;
	ret.origin.y = inner.origin.y + outer.origin.y;
	ret.size.width = MIN(inner.size.width, outer.size.width);
	ret.size.height = MIN(inner.size.height, outer.size.height);
	return ret;
}

Rect convert_frame(View* view, Rect frame) {
	if (!view) return frame;

	Rect ret = convert_rect(view->frame, frame);
	return ret;
}

Rect absolute_frame(Screen* screen, View* view) {
	Rect ret = view->frame;
	//find root view
	View* v = view;
	while (v->superview) {
		v = v->superview;
		ret = convert_frame(v, ret);
	}
	//find containing window
	Window* win = containing_window_int(screen, v);
	return convert_rect(win->frame, ret);
}

void draw_label(Screen* screen, Label* label) {
	View* superview = label->superview;
	ASSERT(superview, "label had no superview!");
	
	if (!label || !label->needs_redraw) return;
	if (superview && !superview->needs_redraw) return;

	label->needs_redraw = 1;
	dirtied = 1;

	Rect frame = absolute_frame(screen, label);

	int idx = 0;
	char* str = label->text;
	int x = frame.origin.x;
	int y = frame.origin.y;
	while (str[idx] != NULL) {
		//go to next line if necessary
		if ((x + CHAR_WIDTH + CHAR_PADDING) > (frame.origin.x + frame.size.width) || str[idx] == '\n') {
			x = frame.origin.x;

			//quit if going to next line would exceed view bounds
			if ((y + CHAR_WIDTH + CHAR_PADDING) > (frame.origin.y + frame.size.height)) break;

			y += CHAR_HEIGHT + CHAR_PADDING;
		}

		draw_char(screen, screen->font, str[idx], x, y, label->text_color);
		
		x += CHAR_WIDTH + CHAR_PADDING;

		idx++;
	}

	label->needs_redraw = 0;
}

void draw_image(Screen* screen, Image* image) {
	View* superview = image->superview;
	if (!image || !image->needs_redraw) return;
	if (superview && !superview->needs_redraw) return;

	image->needs_redraw = 1;
	dirtied = 1;

	Rect frame = absolute_frame(screen, image);

	//iterate through every pixel in the bitmap and draw it
	int num_pixels = frame.size.width * frame.size.height;
	for (int i = 0; i < num_pixels; i++) {
		/*
		int x = frame.origin.x + (i % frame.size.width);
		int y = frame.origin.y + (i / frame.size.height);
		putpixel(screen, x, y, image->bitmap[i]); 
		*/
	}

	image->needs_redraw = 0;
}

void draw_view(Screen* screen, View* view) {
	View* superview = view->superview;
	Window* superwindow = containing_window_int(screen, view);

	if (!view || !view->needs_redraw) return;
	if (superview && !superview->needs_redraw) return;
	if (superwindow && !superwindow->needs_redraw) return;

	//inform subviews that we're being redrawn
	view->needs_redraw = 1;
	dirtied = 1;
	
	Rect frame = absolute_frame(screen, view);

	//fill view with its background color
	draw_rect(screen, frame, view->background_color, THICKNESS_FILLED);

	//draw any labels this view has
	for (unsigned i = 0; i < view->labels.size; i++) {
		Label* label = (Label*)array_m_lookup(i, &(view->labels));
		draw_label(screen, label);
	}

	//draw any images this view has
	for (unsigned i = 0; i < view->images.size; i++) {
		Image* image = (Image*)array_m_lookup(i, &(view->images));
		draw_image(screen, image);
	}

	//draw each subview of this view
	for (unsigned i = 0; i < view->subviews.size; i++) {
		View* subview = (View*)array_m_lookup(i, &(view->subviews));
		draw_view(screen, subview);
	}

	view->needs_redraw = 0;
}

void draw_window(Screen* screen, Window* window) {
	if (!window->needs_redraw && !window->content_view->needs_redraw && !window->title_view->needs_redraw			) return;

	window->needs_redraw = 1;
	dirtied = 1;

	//paint navy blue window
	draw_rect(screen, window->frame, window->border_color, window->border_width);
	
	//only draw a title bar if title_view exists
	if (window->title_view) {
		draw_view(screen, window->title_view);
	}

	//put a small red square in top left corner of the window
	Rect close_button = rect_make(window->frame.origin, size_make(5, 5));
	draw_rect(screen, close_button, color_make(255, 0, 0), THICKNESS_FILLED);

	//only draw the content view if content_view exists
	if (window->content_view) {
		draw_view(screen, window->content_view);
	}
	
	window->needs_redraw = 0;
}

void add_taskbar(Screen* screen) {
	Size taskbar_size = size_make(screen->window->frame.size.width, screen->window->frame.size.height * 0.05);
	Coordinate taskbar_origin = point_make(0, screen->window->frame.size.height - taskbar_size.height);
	View* taskbar_view = create_view(rect_make(taskbar_origin, taskbar_size));
	taskbar_view->background_color = color_make(11, 136, 155);
	add_subview(screen->window->content_view, taskbar_view);

	Coordinate name_label_origin = point_make(taskbar_view->frame.size.width * 0.925, taskbar_view->frame.size.height / 2 - (CHAR_HEIGHT / 2));
	Rect label_rect = rect_make(name_label_origin, size_make(taskbar_size.width - name_label_origin.x, taskbar_size.height));
	Label* name_label = create_label(label_rect, "axle os");
	add_sublabel(taskbar_view, name_label);
}

void draw_desktop(Screen* screen) {
	//paint root desktop
	draw_window(screen, screen->window);
	
	//paint every child window
	for (unsigned i = 0; i < screen->window->subviews.size; i++) {
		Window* win = (Window*)(array_m_lookup(i, &(screen->window->subviews)));
		draw_window(screen, win);
	}
}

void desktop_setup(Screen* screen) {
	screen->window->content_view->background_color = color_make(192, 192, 192);
	add_taskbar(screen);
}

char xserv_draw(Screen* screen) {
	screen->finished_drawing = 0;

	dirtied = 0;
	draw_desktop(screen);

	screen->finished_drawing = 1;

	return dirtied;
}
