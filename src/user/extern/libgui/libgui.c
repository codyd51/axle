#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <kernel/adi.h>

#include <libamc/libamc.h>
#include <stdlibadd/array.h>
#include <stdlibadd/assert.h>

#include <agx/lib/size.h>
#include <agx/lib/screen.h>
#include <agx/lib/shapes.h>
#include <agx/lib/putpixel.h>

#include <awm/awm.h>

#include "libgui.h"

/* Shims */

// From libport

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a <= _b ? _a : _b; })

static void _noop() {}

// Many graphics lib functions call gfx_screen() 
Screen _screen = {0};
Screen* gfx_screen() {
	return &_screen;
}

/* Windows */

gui_window_t* gui_window_create(char* window_title, uint32_t width, uint32_t height) {
	// Ask awm to make a window for us
	amc_msg_u32_3__send(AWM_SERVICE_NAME, AWM_REQUEST_WINDOW_FRAMEBUFFER, width, height);

	// And get back info about the window it made
	amc_message_t* receive_framebuf;
	amc_message_await(AWM_SERVICE_NAME, &receive_framebuf);
	uint32_t event = amc_msg_u32_get_word(receive_framebuf, 0);
	if (event != AWM_CREATED_WINDOW_FRAMEBUFFER) {
		printf("Invalid state. Expected framebuffer command\n");
	}
	uint32_t framebuffer_addr = amc_msg_u32_get_word(receive_framebuf, 1);

	printf("Received framebuffer from awm: %d 0x%08x\n", event, framebuffer_addr);
	uint8_t* buf = (uint8_t*)framebuffer_addr;

	// TODO(PT): Use an awm command to get screen info
	_screen.resolution = size_make(1920, 1080);
	_screen.physbase = (uint32_t*)0;
	_screen.bits_per_pixel = 32;
	_screen.bytes_per_pixel = 4;

	ca_layer* dummy_layer = calloc(1, sizeof(ca_layer));
	dummy_layer->size = _screen.resolution;
	dummy_layer->raw = (uint8_t*)framebuffer_addr;
	dummy_layer->alpha = 1.0;
    _screen.vmem = dummy_layer;

	gui_window_t* window = calloc(1, sizeof(gui_window_t));
	window->_interrupt_cbs = array_create(64);
	window->size = size_make(width, height);
	window->layer = dummy_layer;
	window->text_inputs = array_create(32);
	window->text_views = array_create(32);
	window->views = array_create(32);
	window->timers = array_create(32);
	window->all_gui_elems = array_create(64);

	// Ask awm to set the window title
	uint32_t len = strlen(window_title);
	awm_window_title_msg_t update_title = {.event = AWM_UPDATE_WINDOW_TITLE, .len = len};
	memcpy(update_title.title, window_title, len);
	amc_message_construct_and_send(AWM_SERVICE_NAME, &update_title, sizeof(awm_window_title_msg_t));

	return window;
}

Size _gui_screen_resolution(void) {
	return _screen.resolution;
}

/* Event Loop */

static void _handle_key_down(gui_window_t* window, char ch) {
	// Decide which element to route the keypress to
	// For now, always direct it to the first text input
	// TODO(PT): Model cursor position and use it to display the active text box
	if (window->text_inputs->size == 0) {
		return;
	}
	if (!window->hover_elem) {
		return;
	}
	if (window->hover_elem->base.type != GUI_TYPE_TEXT_INPUT) {
		return;
	}
	text_input_t* active_text_input = &window->hover_elem->ti;
	if (ch == '\b') {
		if (active_text_input->len > 0) {
			char deleted_char = active_text_input->text[active_text_input->len - 1];
			active_text_input->text[--active_text_input->len] = '\0';

			text_box_t* text_box = active_text_input->text_box;

			uint32_t drawn_char_width = text_box->font_size.width + text_box->font_padding.width;
			uint32_t delete_width = drawn_char_width;
			// If the last character was a tab character, we actually need to emplace 4 spaces
			if (deleted_char == '\t') {
				delete_width = drawn_char_width * 4;
			}

			// Move the cursor back before the deleted character
			text_box->cursor_pos.x -= delete_width;
			// Cover it up with a space
			text_box_putchar(text_box, ' ', text_box->background_color);
			// And move the cursor before the space
			text_box->cursor_pos.x -= drawn_char_width;
		}
	}
	else {
		// Draw the character into the text input
		if (active_text_input->len + 1 >= active_text_input->max_len) {
			uint32_t new_max_len = active_text_input->max_len * 2;
			printf("Resizing text input %d -> %d\n", active_text_input->max_len, new_max_len);
			active_text_input->text = realloc(active_text_input->text, new_max_len);
			active_text_input->max_len = new_max_len;
		}
		active_text_input->text[active_text_input->len++] = ch;
		text_box_putchar(active_text_input->text_box, ch, color_black());
	}

	// Inform the text input that it's received a character
	if (active_text_input->text_entry_cb != NULL) {
		active_text_input->text_entry_cb(active_text_input, ch);
	}
}

static void _handle_mouse_moved(gui_window_t* window, awm_mouse_moved_msg_t* moved_msg) {
	Point mouse_pos = point_make(moved_msg->x_pos, moved_msg->y_pos);
	// Iterate backwards to respect z-order
	for (int32_t i = window->all_gui_elems->size - 1; i >= 0; i--) {
		gui_elem_t* elem = array_lookup(window->all_gui_elems, i);
		if (rect_contains_point(elem->base.frame, mouse_pos)) {
			if (elem->base.type == GUI_TYPE_VIEW) {
				// Check if we should instead route to a subview
				for (uint32_t j = 0; j < elem->v.subviews->size; j++) {
					gui_elem_t* sub_elem = array_lookup(elem->v.subviews, j);
					Rect r = sub_elem->base.frame;
					Point local_mouse = point_make(
						mouse_pos.x - rect_min_x(elem->v.content_layer_frame), 
						mouse_pos.y - rect_min_y(elem->v.content_layer_frame)
					);
					if (rect_contains_point(sub_elem->base.frame, local_mouse)) {
						// printf("Gave mouse event to subview <%d 0x%08x>\n", sub_elem->base.type, sub_elem);
						elem = sub_elem;
						break;
					}
				}
				// TODO(PT): We should repeat the above step to handle recursively nested views
			}
			// Was the mouse already inside this element?
			if (window->hover_elem == elem) {
				Rect r = elem->ti.frame;
				//printf("Move within hover elem 0x%08x %d %d in %d %d %d %d\n", elem, mouse_pos.x, mouse_pos.y, r.origin.x, r.origin.y, r.size.width, r.size.height);
				elem->ti._priv_mouse_moved_cb(elem, mouse_pos);
				return;
			}
			else {
				// Exit the previous hover element
				if (window->hover_elem) {
					//printf("Mouse exited previous hover elem 0x%08x\n", window->hover_elem);
					window->hover_elem->base._priv_mouse_exited_cb(window->hover_elem);
					window->hover_elem = NULL;
				}
				//printf("Mouse entered new hover elem 0x%08x\n", elem);
				window->hover_elem = elem;
				elem->base._priv_mouse_entered_cb(elem);
				return;
			}
		}
	}
}

static void _handle_mouse_dragged(gui_window_t* window, awm_mouse_dragged_msg_t* moved_msg) {
	Point mouse_pos = point_make(moved_msg->x_pos, moved_msg->y_pos);
	// Iterate backwards to respect z-order
	if (window->hover_elem) {
		window->hover_elem->base._priv_mouse_dragged_cb(window->hover_elem, mouse_pos);
	}
}

static void _handle_mouse_left_click(gui_window_t* window, Point click_point) {
	if (window->hover_elem) {
		printf("Left click on hover elem 0x%08x\n", window->hover_elem);
		window->hover_elem->base._priv_mouse_left_click_cb(window->hover_elem, click_point);
	}
}

static void _handle_mouse_left_click_ended(gui_window_t* window, Point click_point) {
	if (window->hover_elem) {
		window->hover_elem->ti._priv_mouse_left_click_ended_cb(window->hover_elem, click_point);
	}
}

static void _handle_mouse_exited(gui_window_t* window) {
	// Exit the previous hover element
	if (window->hover_elem) {
		printf("Mouse exited previous hover elem 0x%08x\n", window->hover_elem);
		window->hover_elem->ti._priv_mouse_exited_cb(window->hover_elem);
		window->hover_elem = NULL;
	}
}

static void _handle_mouse_scrolled(gui_window_t* window, awm_mouse_scrolled_msg_t* msg) {
	if (window->hover_elem) {
		window->hover_elem->ti._priv_mouse_scrolled_cb(window->hover_elem, msg->delta_z);
	}
}

typedef struct int_descriptor {
	uint32_t int_no;
	gui_interrupt_cb_t cb;
} int_descriptor_t;

static void _handle_amc_messages(gui_window_t* window) {
	// Deduplicate multiple resize messages in one event-loop pass
	bool got_resize_msg = false;
	awm_window_resized_msg_t newest_resize_msg = {0};

	do {
		amc_message_t* msg;
		amc_message_await_any(&msg);

		// Allow libamc to handle watchdogd pings
		if (libamc_handle_message(msg)) {
			continue;
		}

		// Handle awm messages
		else if (!strncmp(msg->source, AWM_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
			uint32_t event = amc_msg_u32_get_word(msg, 0);
			if (event == AWM_KEY_DOWN) {
				char ch = (char)amc_msg_u32_get_word(msg, 1);
				_handle_key_down(window, ch);
			}
			else if (event == AWM_MOUSE_MOVED) {
				awm_mouse_moved_msg_t* m = (awm_mouse_moved_msg_t*)msg->body;
				_handle_mouse_moved(window, m);
			}
			else if (event == AWM_MOUSE_DRAGGED) {
				awm_mouse_dragged_msg_t* m = (awm_mouse_dragged_msg_t*)msg->body;
				_handle_mouse_dragged(window, m);
			}
			else if (event == AWM_MOUSE_LEFT_CLICK) {
				awm_mouse_left_click_msg_t* m = (awm_mouse_left_click_msg_t*)msg->body;
				_handle_mouse_left_click(window, m->click_point);
			}
			else if (event == AWM_MOUSE_LEFT_CLICK_ENDED) {
				awm_mouse_left_click_ended_msg_t* m = (awm_mouse_left_click_ended_msg_t*)msg->body;
				_handle_mouse_left_click_ended(window, m->click_end_point);
			}
			else if (event == AWM_MOUSE_EXITED) {
				_handle_mouse_exited(window);
			}
			else if (event == AWM_MOUSE_SCROLLED) {
				awm_mouse_scrolled_msg_t* m = (awm_mouse_scrolled_msg_t*)msg->body;
				_handle_mouse_scrolled(window, m);
			}
			else if (event == AWM_WINDOW_RESIZED) {
				got_resize_msg = true;
				awm_window_resized_msg_t* m = (awm_window_resized_msg_t*)msg->body;
				newest_resize_msg = *m;
			}
			continue;
		}

		// Dispatch any other message
		else {
			if (window->_amc_handler != NULL) {
				window->_amc_handler(window, msg);
			}
		}
	} while (amc_has_message());

	if (got_resize_msg) {
		awm_window_resized_msg_t* m = (awm_window_resized_msg_t*)&newest_resize_msg;
		window->size = m->new_size;
		for (uint32_t i = 0; i < window->all_gui_elems->size; i++) {
			gui_elem_t* elem = array_lookup(window->all_gui_elems, i);
			elem->ti._priv_window_resized_cb(elem, window->size);
			if (elem->base.type == GUI_TYPE_VIEW) {
				for (uint32_t j = 0; j < elem->v.subviews->size; j++) {
					gui_elem_t* sub_elem = array_lookup(elem->v.subviews, j);
					sub_elem->base._priv_window_resized_cb(sub_elem, window->size);
				}
			}
		}
	}
}

static void _process_amc_messages(gui_window_t* window) {
	if (window->_interrupt_cbs->size) {
		assert(window->_interrupt_cbs->size == 1, "Only 1 interrupt supported");
		int_descriptor_t* t = array_lookup(window->_interrupt_cbs, 0);
		bool awoke_for_interrupt = adi_event_await(t->int_no);
		if (awoke_for_interrupt) {
			t->cb(window, t->int_no);
			return;
		}
	}
	_handle_amc_messages(window);
}

static void _redraw_dirty_elems(gui_window_t* window) {
	uint32_t start = ms_since_boot();
	for (uint32_t i = 0; i < window->all_gui_elems->size; i++) {
		gui_elem_t* elem = array_lookup(window->all_gui_elems, i);
		bool is_active = window->hover_elem == elem;

		//if (elem->base._priv_needs_display) {
			elem->base._priv_draw_cb(elem, is_active);
			elem->base._priv_needs_display = false;
		//}
	}
	uint32_t end = ms_since_boot();
	uint32_t t = end - start;
	if (t > 2) {
		printf("[%d] libgui draw took %dms\n", getpid(), t);
	}

	// Ask awm to update the window
	amc_msg_u32_1__send(AWM_SERVICE_NAME, AWM_WINDOW_REDRAW_READY);
}

void gui_enter_event_loop(gui_window_t* window) {
	// Draw everything once so the window shows its contents before we start 
	// processing messages
	_redraw_dirty_elems(window);
	while (true) {
		// Process any events sent to this service
		_process_amc_messages(window);
		// Dispatch any ready timers
		gui_dispatch_ready_timers(window);
		// Redraw any dirty elements
		_redraw_dirty_elems(window);
	}
}

void gui_add_interrupt_handler(gui_window_t* window, uint32_t int_no, gui_interrupt_cb_t cb) {
	//hash_map_put(window->_interrupt_cbs, &int_no, sizeof(uint32_t), cb);
	int_descriptor_t* d = calloc(1, sizeof(int_descriptor_t));
	d->int_no = int_no;
	d->cb = cb;
	array_insert(window->_interrupt_cbs, d);
}

void gui_add_message_handler(gui_window_t* window, gui_amc_message_cb_t cb) {
	if (window->_amc_handler != NULL) {
		assert(0, "Only one amc handler is supported");
	}
	window->_amc_handler = cb;
}
