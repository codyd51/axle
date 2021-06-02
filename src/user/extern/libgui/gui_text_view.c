#include <string.h>

#include "gui_text_view.h"
#include "gui_scrollbar.h"
#include "libgui.h"
#include "utils.h"

static void _noop() {}

gui_text_view_t* gui_text_view_alloc(void) {
	gui_text_view_t* v = calloc(1, sizeof(gui_text_view_t));
	gui_scroll_view_alloc_dynamic_fields((gui_scroll_view_t*)v);
	return v;
}

void gui_text_view_init(gui_text_view_t* view, gui_window_t* window, gui_window_resized_cb_t sizer_cb) {
	gui_scroll_view_init((gui_scroll_view_t*)view, window, sizer_cb);
	view->font_size = size_make(8, 12);
	view->font_inset = size_make(4, 6);
	view->cursor_pos = point_make(
		view->font_inset.width,
		view->font_inset.height
	);
	view->controls_content_layer = true;
}

void gui_text_view_add_subview(gui_view_t* superview, gui_text_view_t* subview) {
	gui_scroll_view_add_subview(superview, (gui_scroll_view_t*)subview);
}

void gui_text_view_add_to_window(gui_text_view_t* view, gui_window_t* window) {
	gui_scroll_view_add_to_window((gui_scroll_view_t*)view, window);
}

gui_text_view_t* gui_text_view_create(gui_window_t* window, gui_window_resized_cb_t sizer_cb) {
	gui_text_view_t* view = gui_text_view_alloc();
	gui_text_view_init(view, window, sizer_cb);
	gui_text_view_add_to_window(view, window);
	return view;
}

void gui_text_view_clear(gui_text_view_t* text_view) {
	gui_layer_draw_rect(
		text_view->content_layer,
		rect_make(
			point_zero(),
			text_view->content_layer->scroll_layer.inner->size
		),
		text_view->background_color,
		THICKNESS_FILLED
	);
	text_view->cursor_pos = point_make(
		text_view->font_inset.width,
		text_view->font_inset.height
	);
	text_view->content_layer->scroll_layer.max_y = text_view->font_inset.height;
}

static void _gui_text_view_newline(gui_text_view_t* text_view) {
	text_view->cursor_pos.x = text_view->font_inset.width;
	text_view->cursor_pos.y += text_view->font_size.height + (text_view->font_size.height / 2);

	if (text_view->cursor_pos.y + text_view->font_size.height >= text_view->content_layer->scroll_layer.inner->size.height - (text_view->font_inset.width * 2)) {
		//printf("Text box exceeded scrolling layer, reset to top... (%d)\n", text_box->history->count);
		//text_box_clear_and_erase_history(text_box);
		gui_text_view_clear(text_view);
	}
}

void gui_text_view_putchar(gui_text_view_t* text_view, char ch, Color color) {
	/*
	if (text_box->cursor_pos.y + text_box->font_size.height + text_box->font_padding.height >= text_box->scroll_layer->full_size.height) {
		return;
	}
	*/

	if (ch == '\n') {
		_gui_text_view_newline(text_view);
	}
	else if (ch == '\t') {
		for (int i = 0; i < 4; i++) {
			gui_text_view_putchar(text_view, ' ', color);
		}
	}
	else {
		gui_layer_draw_char(
			text_view->content_layer,
			ch,
			text_view->cursor_pos.x,
			text_view->cursor_pos.y,
			color,
			text_view->font_size
		);
		text_view->cursor_pos.x += text_view->font_size.width;
		/*
		if (text_box->cursor_pos.x + text_box->font_size.width + text_box->font_padding.width >= text_box->size.width) {
			_newline(text_box);
		}
		*/
	}
}

void gui_text_view_puts(gui_text_view_t* text_view, const char* str, Color color) {
	for (int i = 0; i < strlen(str); i++) {
		gui_text_view_putchar(text_view, str[i], color);
	}
}
