#include "view.h"
#include <std/kheap.h>
#include <gfx/lib/shapes.h>
#include <stddef.h>
#include <kernel/util/vfs/fs.h>
#include <std/printf.h>
#include "shader.h"

#define MAX_ELEMENTS 64 

ca_layer* create_layer(Size size) {
	ca_layer* ret = kmalloc(sizeof(ca_layer));
	ret->size = size;
	ret->raw = kmalloc(size.width * size.height * gfx_bpp());
	return ret;
}

View* create_view(Rect frame) {
	View* view = (View*)kmalloc(sizeof(View));
	view->layer = create_layer(frame.size);
	view->frame = frame;
	view->superview = NULL;
	view->background_color = color_make(0, 255, 0);
	view->subviews = array_m_create(MAX_ELEMENTS);
	view->labels = array_m_create(MAX_ELEMENTS);
	view->bmps = array_m_create(MAX_ELEMENTS);
	view->shaders = array_m_create(MAX_ELEMENTS);
	view->needs_redraw = 1;
	return view;
}

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

static View* create_content_view(Window* window) {
	if (!window) return NULL;

	int title_height = 20;
	if (window->title_view) title_height = window->title_view->frame.size.height;

	Rect inner_frame = rect_make(point_make((window->border_width * 2), title_height), size_make(window->frame.size.width - (window->border_width * 4), window->frame.size.height - title_height - (window->border_width * 2)));
	View* content_view = create_view(inner_frame);
	content_view->background_color = color_make(255, 255, 255);
	
	return content_view;
}

Window* create_window(Rect frame) {
	Window* window = (Window*)kmalloc(sizeof(Window));
	memset(window, 0, sizeof(Window));

	window->layer = create_layer(frame.size);
	window->size = frame.size;
	window->frame = frame;
	window->border_color = color_make(120, 245, 80);
	window->border_width = 1;
	window->subviews = array_m_create(MAX_ELEMENTS);
	window->title = "Window";

	window->title_view = create_title_view(window);
	window->content_view = create_content_view(window);

	window->needs_redraw = 1;
		
	return window;
}

Label* create_label(Rect frame, char* text) {
	Label* label = (Label*)kmalloc(sizeof(Label));
	label->layer = create_layer(frame.size);
	label->frame = frame;
	label->superview = NULL;
	label->text_color = color_black();
	label->needs_redraw = 1;

	label->text = strdup(text);
	return label;
}

Button* create_button(Rect frame, char* text) {
	View* button = create_view(frame);

	Label* title = create_label(frame, text);
	add_sublabel(button, title);

	button->needs_redraw = 1;
	return (Button*)button;
}

Bmp* create_bmp(Rect frame, ca_layer* layer) {
	if (!layer) return NULL;

	Bmp* bmp = (Bmp*)kmalloc(sizeof(Bmp));
	bmp->frame = frame;
	bmp->layer = layer;
	bmp->needs_redraw = 1;
	return bmp;
}

Bmp* load_bmp(Rect frame, char* filename) {
	FILE* file = fopen(filename, (char*)"");
	if (!file) {
		printf_err("File %s not found! Not loading BMP", filename);
		return NULL;
	}

	unsigned char header[54];
	fread(header, sizeof(char), 54, file);

	//get width and height from header
	int width = *(int*)&header[18];
	int height = *(int*)&header[22];
	printf_info("loading BMP with dimensions (%d,%d)", width, height);

	int bpp = gfx_bpp();
	ca_layer* layer = create_layer(size_make(width, height));
	//image is upside down in memory so build array from bottom up
	for (int i = width * height; i >= 0; i--) {
		int idx = i * bpp;
		//we process 3 bytes at a time because image is stored in BGR, we need RGB
		layer->raw[idx + 0] = fgetc(file);
		layer->raw[idx + 1] = fgetc(file);
		layer->raw[idx + 2] = fgetc(file);
		//fourth byte is for alpha channel if we used 32bit BMPs
		//we only use 24bit, so don't try to read it
		//fgetc(file);
	}

	Bmp* bmp = create_bmp(frame, layer);
	return bmp;
}

void mark_needs_redraw(View* view) {
	if (!view) return;

	//if this view has already been marked, quit
	if (view->needs_redraw) return;

	view->needs_redraw = 1;
	if (view->superview && view->superview->superview) {
		mark_needs_redraw(view->superview);
	}
}

void add_sublabel(View* view, Label* label) {
	if (!view || !label) return;

	array_m_insert(view->labels, label);
	label->superview = view;
	label->needs_redraw = 1;
	mark_needs_redraw(view);
}

void remove_sublabel(View* view, Label* label) {
	if (!view || !label) return;

	array_m_remove(view->labels, array_m_index(view->labels, label));
	label->superview = NULL;
	label->needs_redraw = 1;
	mark_needs_redraw(view);
}

void add_subview(View* view, View* subview) {
	if (!view || !subview) return;

	array_m_insert(view->subviews, subview);
	subview->superview = view;
	mark_needs_redraw(view);
}

void remove_subview(View* view, View* subview) {
	if (!view || !subview) return;

	array_m_remove(view->subviews, array_m_index(view->subviews, subview));
	subview->superview = NULL;
	subview->needs_redraw = 1;
	mark_needs_redraw(view);
}

void add_bmp(View* view, Bmp* bmp) {
	if (!view || !bmp) return;

	array_m_insert(view->bmps, bmp);
	bmp->superview = view;
	mark_needs_redraw(view);
}

void remove_bmp(View* view, Bmp* bmp) {
	if (!view || !bmp) return;

	array_m_remove(view->bmps, array_m_index(view->bmps, bmp));
	bmp->superview = NULL;
	mark_needs_redraw(view);
}

void add_shader(View* view, Shader* s) {
	if (!view || !s) return;

	array_m_insert(view->shaders, s);
	s->superview = view;
	mark_needs_redraw(view);
	compute_shader(s);
}

void remove_shader(View* view, Shader* s) {
	if (!view || !s) return;

	array_m_remove(view->shaders, array_m_index(view->shaders, s));
	s->superview = NULL;
	mark_needs_redraw(view);
}

void set_background_color(View* view, Color color) {
	if (!view) return;

	view->background_color = color;
	mark_needs_redraw(view);
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

void set_frame(View* view, Rect frame) {
	if (!view) return;

	Rect old_frame = view->frame;
	view->frame = frame;

	//resize layer
	int layer_bytes = old_frame.size.width * old_frame.size.height;
	//realloc(view->layer, layer_bytes);
	
	mark_needs_redraw(view);
}
