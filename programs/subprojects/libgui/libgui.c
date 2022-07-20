#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <kernel/adi.h>

#include <libamc/libamc.h>
#include <libutils/array.h>
#include <libutils/assert.h>

#include <libagx/lib/screen.h>
#include <libagx/lib/shapes.h>
#include <libagx/lib/putpixel.h>

#include <awm/awm_messages.h>

#include "libgui.h"
#include "utils.h"

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

// Some agx functions call gfx_screen() 
Screen _screen = {0};
Screen* gfx_screen() {
	return &_screen;
}

static gui_application_t* _g_application = NULL;

/* Windows */

gui_window_t* gui_window_create(char* window_title, uint32_t width, uint32_t height) {
	if (_g_application == NULL) {
		printf("Calling gui_application_create() on behalf of root gui_window_t\n");
		gui_application_create();
	}

	// Ask awm to make a window for us
	awm_create_window_request_t req = {
		.event = AWM_CREATE_WINDOW_REQUEST,
		.window_size = size_make(width, height)
	};
	amc_message_send(AWM_SERVICE_NAME, &req, sizeof(req));

	// And get back info about the window it made
	amc_message_t* msg;
	amc_message_await__u32_event(AWM_SERVICE_NAME, AWM_CREATE_WINDOW_RESPONSE, &msg);
	awm_create_window_response_t* resp = &msg->body;
	uintptr_t framebuffer_addr = resp->framebuffer;

	printf("Received framebuffer from awm: %p\n", framebuffer_addr);
	uint8_t* buf = (uint8_t*)framebuffer_addr;

	// TODO(PT): Use an awm command to get screen info
	_screen.resolution = resp->screen_resolution;
	_screen.bits_per_pixel = resp->bytes_per_pixel * 8;
	_screen.bytes_per_pixel = resp->bytes_per_pixel;

	ca_layer* dummy_layer = calloc(1, sizeof(ca_layer));
	dummy_layer->size = _screen.resolution;
	dummy_layer->raw = (uint8_t*)framebuffer_addr;
	dummy_layer->alpha = 1.0;
    _screen.vmem = dummy_layer;

	gui_layer_t* dummy_gui_layer = calloc(1, sizeof(gui_layer_t));
	dummy_gui_layer->fixed_layer.type = GUI_FIXED_LAYER;
	dummy_gui_layer->fixed_layer.inner = dummy_layer;

	gui_window_t* window = calloc(1, sizeof(gui_window_t));
	window->size = size_make(width, height);
	window->layer = dummy_gui_layer;
	window->views = array_create(32);
	window->all_gui_elems = array_create(128);

	gui_set_window_title(window_title);

	array_insert(_g_application->windows, window);

	return window;
}

void gui_set_window_title(char* window_title) {
	// Ask awm to set the window title
	uint32_t len = strlen(window_title);
	awm_window_title_msg_t update_title = {.event = AWM_UPDATE_WINDOW_TITLE, .len = len};
	memcpy(update_title.title, window_title, len);
	amc_message_send(AWM_SERVICE_NAME, &update_title, sizeof(awm_window_title_msg_t));
}

struct mallinfo_s {
	int arena;    /* total space allocated from system */
	int ordblks;  /* number of non-inuse chunks */
	int smblks;   /* unused -- always zero */
	int hblks;    /* number of mmapped regions */
	int hblkhd;   /* total space in mmapped regions */
	int usmblks;  /* unused -- always zero */
	int fsmblks;  /* unused -- always zero */
	int uordblks; /* total allocated space */
	int fordblks; /* total non-inuse space */
	int keepcost; /* top-most, releasable (via malloc_trim) space */
};	
struct mallinfo_s mallinfo();

void print_memory(void) {
	struct mallinfo_s p = mallinfo();
	printf("Heap space: 		 0x%08x\n", p.arena);
	printf("Total allocd space : 0x%08x\n", p.uordblks);
	printf("Total free space   : 0x%08x\n", p.fordblks);
}

static void gui_window_teardown(gui_window_t* window) {
	free(_screen.vmem);
	_screen.vmem = NULL;
	free(window->layer);

	while (window->views->size) {
		gui_view_t* v = array_lookup(window->views, 0);
		array_remove(window->views, 0);
		assert(v->parent_layer == window->layer, "not root view");
		uint32_t idx = array_index(window->all_gui_elems, v);
		array_remove(window->all_gui_elems, idx);
		gui_view_destroy(v);
	}
	array_destroy(window->views);

	assert(window->all_gui_elems->size == 0, "not zero all");
	array_destroy(window->all_gui_elems);

	free(window);

	printf("** Frees done\n");
	print_memory();

	// Ask awm to update the window
	amc_msg_u32_1__send(AWM_SERVICE_NAME, AWM_CLOSE_WINDOW);
}

void gui_application_teardown(gui_application_t* app) {
	for (int32_t i = 0; i < app->windows->size; i++) {
		gui_window_t* window = array_lookup(app->windows, i);
		gui_window_teardown(window);
	}
	array_destroy(app->windows);

	assert(app->_interrupt_cbs->size == 0, "not zero cbs");
	array_destroy(app->_interrupt_cbs);

	for (int32_t i = 0; i < app->timers->size; i++) {
		gui_timer_t* t = array_lookup(app->timers, i);
		free(t);
	}
	array_destroy(app->timers);

	free(app);
}

Size _gui_screen_resolution(void) {
	return _screen.resolution;
}

/* Event Loop */

static void _handle_key_up(gui_window_t* window, uint32_t ch) {
	if (!window->hover_elem) {
		return;
	}

	// Dispatch the key down event handler of the active element
	window->hover_elem->base._priv_key_up_cb(window->hover_elem, ch);
}

static void _handle_key_down(gui_window_t* window, uint32_t ch) {
	if (!window->hover_elem) {
		return;
	}

	// Dispatch the key down event handler of the active element
	window->hover_elem->base._priv_key_down_cb(window->hover_elem, ch);
}

static void _handle_mouse_moved(gui_window_t* window, awm_mouse_moved_msg_t* moved_msg) {
	Point mouse_pos = point_make(moved_msg->x_pos, moved_msg->y_pos);
	// Iterate backwards to respect z-order
	for (int32_t i = window->all_gui_elems->size - 1; i >= 0; i--) {
		gui_elem_t* elem = array_lookup(window->all_gui_elems, i);
		if (rect_contains_point(elem->base.frame, mouse_pos)) {
			if (elem->base.type == GUI_TYPE_VIEW || elem->base.type == GUI_TYPE_SCROLL_VIEW) {
				elem = elem->v.elem_for_mouse_pos_cb(&elem->v, mouse_pos);
			}
			// Was the mouse already inside this element?
			if (window->hover_elem == elem) {
				Rect r = elem->base.frame;
				if (window->hover_elem->base.type == GUI_TYPE_SLIDER) {
					printf("Translate for slider\n");
					mouse_pos.x -= rect_min_x(window->hover_elem->sl.superview->frame);
					mouse_pos.y -= rect_min_y(window->hover_elem->sl.superview->frame);
				}
				//printf("%d %d - Move within hover_elem %d\n", mouse_pos.x, mouse_pos.y, elem->base.type);
				elem->base._priv_mouse_moved_cb(elem, mouse_pos);
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
		window->hover_elem->base._priv_mouse_left_click_ended_cb(window->hover_elem, click_point);
	}
}

static void _handle_mouse_exited(gui_window_t* window) {
	// Exit the previous hover element
	if (window->hover_elem) {
		printf("Mouse exited previous hover elem 0x%08x\n", window->hover_elem);
		window->hover_elem->base._priv_mouse_exited_cb(window->hover_elem);
		window->hover_elem = NULL;
	}
}

static void _handle_mouse_scrolled(gui_window_t* window, awm_mouse_scrolled_msg_t* msg) {
	// Try and find a scroll view to deliver the scroll to
	for (int32_t i = window->all_gui_elems->size - 1; i >= 0; i--) {
		gui_elem_t* elem = array_lookup(window->all_gui_elems, i);
		if (
			elem->base.type == GUI_TYPE_SCROLL_VIEW &&
			rect_contains_point(elem->base.frame, msg->mouse_pos)
		) {
			elem->base._priv_mouse_scrolled_cb(elem, msg->delta_z);
			return;
		}
	}
}

typedef struct int_descriptor {
	uint32_t int_no;
	gui_interrupt_cb_t cb;
} int_descriptor_t;

static void _handle_amc_messages(gui_application_t* app, bool should_block, bool* did_exit) {
	// Deduplicate multiple resize messages in one event-loop pass
	bool got_resize_msg = false;
	awm_window_resized_msg_t newest_resize_msg = {0};

	if (!should_block) {
		if (!amc_has_message()) {
			return;
		}
	}

	// For now, always pass events to the first window
	gui_window_t* window = NULL;
	if (app->windows->size) {
		window = array_lookup(app->windows, 0);
	}

	// If the application requested batch-delivery of amc messages, queue them now
	array_t* amc_batched_messages = NULL;
	if (app->_amc_batch_handler) {
		amc_batched_messages = array_create(128);
	}

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
			// Handle awm messages that don't require a window handle
			if (event == AWM_CLOSE_WINDOW_REQUEST) {
				*did_exit = true;
				continue;
			}

			// Handle awm messages that do require a window handle
			if (window != NULL) {
				if (event == AWM_KEY_DOWN) {
					uint32_t ch = amc_msg_u32_get_word(msg, 1);
					_handle_key_down(window, ch);
					continue;
				}
				else if (event == AWM_KEY_UP) {
					uint32_t ch = amc_msg_u32_get_word(msg, 1);
					_handle_key_up(window, ch);
					continue;
				}
				else if (event == AWM_MOUSE_MOVED) {
					awm_mouse_moved_msg_t* m = (awm_mouse_moved_msg_t*)msg->body;
					_handle_mouse_moved(window, m);
					continue;
				}
				else if (event == AWM_MOUSE_DRAGGED) {
					awm_mouse_dragged_msg_t* m = (awm_mouse_dragged_msg_t*)msg->body;
					_handle_mouse_dragged(window, m);
					continue;
				}
				else if (event == AWM_MOUSE_LEFT_CLICK) {
					awm_mouse_left_click_msg_t* m = (awm_mouse_left_click_msg_t*)msg->body;
					_handle_mouse_left_click(window, m->click_point);
					continue;
				}
				else if (event == AWM_MOUSE_LEFT_CLICK_ENDED) {
					awm_mouse_left_click_ended_msg_t* m = (awm_mouse_left_click_ended_msg_t*)msg->body;
					_handle_mouse_left_click_ended(window, m->click_end_point);
					continue;
				}
				else if (event == AWM_MOUSE_ENTERED) {
					continue;
				}
				else if (event == AWM_MOUSE_EXITED) {
					_handle_mouse_exited(window);
					continue;
				}
				else if (event == AWM_MOUSE_SCROLLED) {
					awm_mouse_scrolled_msg_t* m = (awm_mouse_scrolled_msg_t*)msg->body;
					_handle_mouse_scrolled(window, m);
					continue;
				}
				else if (event == AWM_WINDOW_RESIZED) {
					got_resize_msg = true;
					awm_window_resized_msg_t* m = (awm_window_resized_msg_t*)msg->body;
					newest_resize_msg = *m;
					continue;
				}
				else if (event == AWM_WINDOW_RESIZE_ENDED) {
					continue;
				}
				else if (event == AWM_CLOSE_WINDOW_REQUEST) {
					*did_exit = true;
					continue;
				}
			}
		}

		// Dispatch any message that wasn't handled above
		if (app->_amc_handler != NULL) {
			app->_amc_handler(msg);
		}
		else if (app->_amc_batch_handler != NULL) {
			// Queue message 
			amc_message_t* copy = calloc(1, sizeof(amc_message_t) + msg->len);
			memcpy(copy, msg, sizeof(amc_message_t) + msg->len);
			array_insert(amc_batched_messages, copy);

			if (amc_batched_messages->size + 1 >= amc_batched_messages->max_size) {
				// Deliver a subset of messages in a batch
				app->_amc_batch_handler(amc_batched_messages);
				for (int32_t i = amc_batched_messages->size - 1; i >= 0; i--) {
					free(array_lookup(amc_batched_messages, i));;
					array_remove(amc_batched_messages, i);
				}
			}
		}
	} while (amc_has_message());

	// If we've batched up a set of amc messages now, deliver them
	if (amc_batched_messages != NULL) {
		if (amc_batched_messages->size) {
			app->_amc_batch_handler(amc_batched_messages);
			for (uint32_t i = 0; i < amc_batched_messages->size; i++) {
				free(array_lookup(amc_batched_messages, i));;
			}
		}
		array_destroy(amc_batched_messages);
	}

	if (got_resize_msg && window != NULL) {
		awm_window_resized_msg_t* m = (awm_window_resized_msg_t*)&newest_resize_msg;
		window->size = m->new_size;
		for (uint32_t i = 0; i < window->all_gui_elems->size; i++) {
			gui_elem_t* elem = array_lookup(window->all_gui_elems, i);
			elem->base._priv_window_resized_cb(elem, window->size);
		}
	}
}

static void _process_amc_messages(gui_application_t* app, bool should_block, bool* did_exit) {
	if (app->_interrupt_cbs->size) {
		assert(app->_interrupt_cbs->size == 1, "Only 1 interrupt supported");
		int_descriptor_t* t = array_lookup(app->_interrupt_cbs, 0);
		bool awoke_for_interrupt = adi_event_await(t->int_no);
		if (awoke_for_interrupt) {
			t->cb(t->int_no);
			return;
		}
	}
	_handle_amc_messages(app, should_block, did_exit);
}

static void _redraw_dirty_elems(gui_window_t* window) {
	uintptr_t start = ms_since_boot();
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
	if (t > 3) {
		//printf("[%d] libgui draw took %dms\n", getpid(), t);
	}

	// Ask awm to update the window
	amc_msg_u32_1__send(AWM_SERVICE_NAME, AWM_WINDOW_REDRAW_READY);
}

typedef enum timers_state {
	TIMERS_LATE = 0,
	SLEPT_FOR_TIMERS = 1,
	NO_TIMERS = 2
} timers_state_t;

static timers_state_t _sleep_for_timers(gui_application_t* app) {
	if (app->timers->size == 0) {
		return NO_TIMERS;
	}
	uint32_t next_fire_date = 0;
	for (uint32_t i = 0; i < app->timers->size; i++) {
        gui_timer_t* t = array_lookup(app->timers, i);
		if (!next_fire_date || t->fires_after < next_fire_date) {
			next_fire_date = t->fires_after;
		}
    }
	int32_t time_to_next_fire = next_fire_date - ms_since_boot();
	if (time_to_next_fire <= 0) {
		return TIMERS_LATE;
	}

	//printf("libgui awaiting next timer that will fire in %d\n", time_to_next_fire);
	uint32_t b[2];
    b[0] = AMC_SLEEP_UNTIL_TIMESTAMP_OR_MESSAGE;
    b[1] = time_to_next_fire;
    amc_message_send(AXLE_CORE_SERVICE_NAME, &b, sizeof(b));

	return SLEPT_FOR_TIMERS;
}

void gui_run_event_loop_pass(bool prevent_blocking, bool* did_exit) {
	timers_state_t timers_state = _sleep_for_timers(_g_application);

	// Unless the caller specified otherwise, only allow blocking for a message if there are no timers queued up
	bool should_block = timers_state == NO_TIMERS;
	if (prevent_blocking) {
		should_block = false;
	}
	_process_amc_messages(_g_application, should_block, did_exit);
	// Dispatch any ready timers
	gui_dispatch_ready_timers(_g_application);
	// Redraw any dirty elements
	for (int32_t i = 0; i < _g_application->windows->size; i++) {
		gui_window_t* window = array_lookup(_g_application->windows, i);
		_redraw_dirty_elems(window);
	}
}

void gui_enter_event_loop(void) {
	printf("Enter event loop\n");
	print_memory();
	// Draw everything once so the window shows its contents before we start 
	// processing messages
	for (int32_t i = 0; i < _g_application->windows->size; i++) {
		gui_window_t* window = array_lookup(_g_application->windows, i);
		_redraw_dirty_elems(window);
	}

	bool did_exit = false;
	while (!did_exit) {
		gui_run_event_loop_pass(false, &did_exit);
	}

	printf("Exited from runloop!\n");
	gui_application_teardown(_g_application);
}

void gui_add_interrupt_handler(uint32_t int_no, gui_interrupt_cb_t cb) {
	//hash_map_put(window->_interrupt_cbs, &int_no, sizeof(uint32_t), cb);
	int_descriptor_t* d = calloc(1, sizeof(int_descriptor_t));
	d->int_no = int_no;
	d->cb = cb;
	array_insert(_g_application->_interrupt_cbs, d);
}

void gui_add_message_handler(gui_amc_message_cb_t cb) {
	if (_g_application->_amc_handler != NULL) {
		assert(0, "Only one amc handler is supported");
	}
	if (_g_application->_amc_batch_handler != NULL) {
		assert(0, "Cannot mix batch and serial message processing");
	}
	_g_application->_amc_handler = cb;
}

void gui_add_message_batch_handler(gui_amc_message_batch_cb_t cb) {
	if (_g_application->_amc_batch_handler != NULL) {
		assert(0, "Only one amc batch handler is supported");
	}
	if (_g_application->_amc_handler != NULL) {
		assert(0, "Cannot mix batch and serial message processing");
	}
	_g_application->_amc_batch_handler = cb;
}

gui_application_t* gui_application_create(void) {
	assert(_g_application == NULL, "An application can call gui_application_create exactly once");

	_g_application = calloc(1, sizeof(gui_application_t));
	_g_application->windows = array_create(8);

	_g_application->timers = array_create(16);
	_g_application->_interrupt_cbs = array_create(8);
	return _g_application;
}

gui_application_t* gui_get_application(void) {
	return _g_application;
}
