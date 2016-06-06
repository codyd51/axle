#include "xserv.h"
#include <std/math.h>

//has the screen been modified this refresh?
static char dirtied = 0;

View* label_superview_int(Screen* screen, Label* label) {
	//traverse view heirarchy, finding the view that contains the label
	for (int i = 0; i < screen->window->subviews.size; i++) {
		Window* window = array_m_lookup(i, &(screen->window->subviews));

		for (int k = 0; k < 2; k++) {
			View* view;
			switch (k) {
				case 0:
					view = window->title_view;
					break;
				case 1:
				default:
					view = window->content_view;
					break;
			}

			if (array_m_index(label, &(view->labels)) != -1) return view;
			for (int j = 0; j < view->subviews.size; j++) {
				View* subview = array_m_lookup(j, &(view->subviews));
				if (array_m_index(label, &(subview->labels)) != -1) return subview;
			}
		}
	}
	ASSERT(0, "Couldn't find label's superview!");
	return NULL;
}

View* image_superview_int(Screen* screen, Image* image) {
	//traverse view hierarchy, finding view which contains image
	for (int i = 0; i < screen->window->subviews.size; i++) {
		Window* window = array_m_lookup(i, &(screen->window->subviews));
		View* view = window->content_view;
		if (array_m_index(image, &(view->images)) != -1) return view;
		for (int j = 0; j < view->subviews.size; i++) {
			View* subview = array_m_lookup(j, &(view->subviews));
			if (array_m_index(image, &(subview->images)) != -1) return subview;
		}
	}
	ASSERT(0, "Couldn't find image's superview!");
	return NULL;
}

Window* containing_window_int(Screen* screen, View* v) {
	//find root window
	View* view = v;
	while (view->superview) {
		view = view->superview;
	}

	//traverse view hierarchy, find window which has view as its title or content view
	if (screen->window->title_view == view || screen->window->content_view == view) return screen->window;
	for (int i = 0; i < screen->window->subviews.size; i++) {
		Window* window = array_m_lookup(i, &(screen->window->subviews));
		if (window->title_view == view || window->content_view == view) return window;
		for (int j = 0; j < window->subviews.size; j++) {
			Window* subwindow = array_m_lookup(j, &(window->subviews));
			if (subwindow->title_view == view || subwindow->content_view == view) return subwindow;
		}
	}
	ASSERT(0, "Couldn't find view's window!");
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

void draw_label(Screen* screen, Label* label) {
	View* superview = label->superview;
	ASSERT(superview, "label had no superview!");
	if (!label->needs_redraw && !superview->needs_redraw) return;

	label->needs_redraw = 1;
	dirtied = 1;

	Rect frame = convert_frame(superview, label->frame);

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
	View* superview = image_superview_int(screen, image);
	if (!image->needs_redraw && !superview->needs_redraw) return;

	image->needs_redraw = 1;
	dirtied = 1;

	Rect frame = convert_frame(superview, image->frame);

	//iterate through every pixel in the bitmap and draw it
	int num_pixels = frame.size.width * frame.size.height;
	for (int i = 0; i < num_pixels; i++) {
		int x = frame.origin.x + (i % frame.size.width);
		int y = frame.origin.y + (i / frame.size.height);
		//putpixel(screen, x, y, image->bitmap[i]); 
	}

	image->needs_redraw = 0;
}

void draw_view(Screen* screen, View* view) {
	View* superview = view->superview;
	Window* superwindow = containing_window_int(screen, view);
	
	if (!view->needs_redraw && !superview->needs_redraw && !superwindow->needs_redraw) return;

	//inform subviews that we're being redrawn
	view->needs_redraw = 1;
	dirtied = 1;

	Rect frame;
	if (superview) frame = convert_frame(superview, view->frame);
	else frame = convert_rect(superwindow->frame, view->frame);

	//fill view with its background color
	draw_rect(screen, frame, view->background_color, THICKNESS_FILLED);

	//draw any labels this view has
	for (int i = 0; i < view->labels.size; i++) {
		Label* label = (Label*)array_m_lookup(i, &(view->labels));
		//draw_label(screen, label);
	}

	//draw any images this view has
	for (int i = 0; i < view->images.size; i++) {
		Image* image = (Image*)array_m_lookup(i, &(view->images));
		draw_image(screen, image);
	}

	//draw each subview of this view
	for (int i = 0; i < view->subviews.size; i++) {
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
	draw_rect(screen, window->frame, window->border_color, 1);
	
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
	Size taskbar_size = size_make(screen->window->size.width, screen->window->size.height * 0.05);
	Coordinate taskbar_origin = point_make(0, screen->window->size.height - taskbar_size.height);
	Window* taskbar_window = create_window(rect_make(taskbar_origin, taskbar_size));
	add_subwindow(screen->window, taskbar_window);

	View* taskbar_view = create_view(rect_make(taskbar_origin, taskbar_size));
	taskbar_window->content_view = taskbar_view;
	taskbar_view->background_color = color_make(11, 136, 155);

	Coordinate name_label_origin = point_make(taskbar_view->frame.size.width * 0.925, taskbar_view->frame.origin.y + taskbar_view->frame.size.height / 2 - (CHAR_HEIGHT / 2));
	Rect label_rect = rect_make(name_label_origin, size_make(taskbar_size.width - name_label_origin.x, taskbar_size.height));
	Label* name_label = create_label(label_rect, "axle os");
	add_sublabel(taskbar_view, name_label);
}

void draw_desktop(Screen* screen) {
	/*
	Coordinate origin = point_make(0, 0);
	Size sz = size_make(screen->window->size.width, screen->window->size.height);
	Rect r = rect_make(origin, sz);
	draw_rect(screen, r, color_make(192, 192, 192), THICKNESS_FILLED);

	add_taskbar(screen);
	*/
	//paint every child window
	for (int i = 0; i < screen->window->subviews.size; i++) {
		Window* win = (Window*)(array_m_lookup(i, &(screen->window->subviews)));
		draw_window(screen, win);
	}
}

char xserv_draw(Screen* screen) {
	screen->finished_drawing = 0;

	dirtied = 0;
	draw_desktop(screen);

	screen->finished_drawing = 1;

	return dirtied;
}
