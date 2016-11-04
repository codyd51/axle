#include "view.h"
#include <std/kheap.h>
#include <gfx/lib/shapes.h>
#include <stddef.h>
#include <kernel/util/vfs/fs.h>
#include <std/printf.h>
#include <std/string.h>
#include <std/memory.h>
#include <gfx/lib/shader.h>

#define MAX_ELEMENTS 64

void view_teardown(View* view) {
	if (!view) return;

	for (int i = 0; i < view->subviews->size; i++) {
		View* view = (View*)array_m_lookup(view->subviews, i);
		view_teardown(view);

		Label* label = (Label*)array_m_lookup(view->labels, i);
		label_teardown(label);

		Bmp* bmp = (Bmp*)array_m_lookup(view->bmps, i);
		bmp_teardown(bmp);

		Shader* s = (Shader*)array_m_lookup(view->shaders, i);
		shader_teardown(s);
	}
	//free subviews array
	array_m_destroy(view->subviews);
	//free sublabels
	array_m_destroy(view->labels);
	//free bmps
	array_m_destroy(view->bmps);
	//free shaders
	array_m_destroy(view->shaders);

	//free backing layer
	layer_teardown(view->layer);
	
	//finally, free view itself
	kfree(view);
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

void set_frame(View* view, Rect frame) {
	if (!view) return;

	Rect old_frame = view->frame;
	old_frame = old_frame; // TODO: do something here
	view->frame = frame;

	//resize layer
	// int layer_bytes = old_frame.size.width * old_frame.size.height;
	//realloc(view->layer, layer_bytes);

	mark_needs_redraw(view);
}
