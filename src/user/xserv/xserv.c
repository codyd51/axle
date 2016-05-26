#include "xserv.h"

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8
#define CHAR_PADDING 2
void draw_label(Screen* screen, Label* label) {
	int idx = 0;
	char* str = label->text;
	int x = label->frame.origin.x;
	int y = label->frame.origin.y;
	while (str[idx] != NULL) {
		//go to next line if necessary
		if ((x + CHAR_WIDTH + CHAR_PADDING) > (label->frame.origin.x + label->frame.size.width) || str[idx] == '\n') {
			x = label->frame.origin.x;
			y += CHAR_HEIGHT + CHAR_PADDING;
		}

		draw_char(screen, screen->font, str[idx], x, y, label->text_color);
		
		x += CHAR_WIDTH + CHAR_PADDING;

		idx++;
	}
}

void draw_image(Screen* screen, Image* image) {
	//iterate through every pixel in the bitmap and draw it
	int num_pixels = image->frame.size.width * image->frame.size.height;
	for (int i = 0; i < num_pixels; i++) {
		int x = image->frame.origin.x + (i % image->frame.size.width);
		int y = image->frame.origin.y + (i / image->frame.size.height);
		putpixel(screen, x, y, image->bitmap[i]); 
	}
}

void draw_view(Screen* screen, View* view) {
	//fill view with its background color
	draw_rect(screen, view->frame, view->background_color, THICKNESS_FILLED);

	//draw any labels this view has
	for (int i = 0; i < view->labels.size; i++) {
		Label* label = (Label*)lookup_mutable_array(i, &(view->labels));
		draw_label(screen, label);
	}

	//draw any images this view has
	for (int i = 0; i < view->images.size; i++) {
		Image* image = (Image*)lookup_mutable_array(i, &(view->images));
		draw_image(screen, image);
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
	Rect label_rect = create_rect(name_label_origin, create_size(taskbar_size.width - name_label_origin.x, taskbar_size.height));
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
