#include "view.h"
#include <std/kheap.h>
#include <gfx/lib/shapes.h>

View* create_view(Rect frame) {
	View* view = kmalloc(sizeof(View));
	view->frame = frame;
	view->background_color = 0x00FF00;
	view->subviews = create_mutable_array(16);
	view->labels = create_mutable_array(16);
	view->images = create_mutable_array(16);
	return view;
}

static View* create_title_view(Window* window) {
	Rect title_view_frame = create_rect(window->frame.origin, create_size(window->frame.size.width, 20));
	View* title_view = create_view(title_view_frame);
	title_view->background_color = window->border_color;

	//add title label to title view
	Rect label_frame = create_rect(create_coordinate(title_view_frame.origin.x + 10, title_view->frame.origin.y + 5), title_view_frame.size);
	Label* title_label = create_label(label_frame, "Test Window");
	title_label->text_color = 0xFFFFFF;
	add_sublabel(title_view, title_label);

	return title_view;
}

static View* create_content_view(Window* window) {
	Rect inner_frame = create_rect(create_coordinate(window->frame.origin.x + 2, window->frame.origin.y + window->title_view->frame.size.height), create_size(window->frame.size.width - 4, window->frame.size.height - window->title_view->frame.size.height - 2));
	View* content_view = create_view(inner_frame);
	content_view->background_color = 0xFFFFFF;
	
	return content_view;
}

Window* create_window(Rect frame) {
	Window* window = kmalloc(sizeof(Window));
	window->size = frame.size;
	window->frame = frame;
	window->border_color = 0x0000FF;
	window->subwindows = create_mutable_array(16);

	window->title_view = create_title_view(window);
	window->content_view = create_content_view(window);
		
	return window;
}

Label* create_label(Rect frame, char* text) {
	Label* label = kmalloc(sizeof(Label));
	label->frame = frame;
	label->text = text;
	label->text_color = 0x000000;
	return label;
}

Image* create_image(Rect frame, uint32_t* bitmap) {
	Image* image = kmalloc(sizeof(Image));
	image->frame = frame;
	image->bitmap = bitmap;
	return image;
}

void add_sublabel(View* view, Label* label) {
	insert_mutable_array(label, &(view->labels));
}

void add_subimage(View* view, Image* image) {
	insert_mutable_array(image, &(view->images));
}

void add_subview(View* view, View* subview) {
	insert_mutable_array(subview, &(view->subviews));
	//subview->superview = view;
}

void add_subwindow(Window* window, Window* subwindow) {
	insert_mutable_array(subwindow, &(window->subwindows));
}

