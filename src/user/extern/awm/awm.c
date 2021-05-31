#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>

#include <agx/font/font.h>
#include <agx/lib/shapes.h>
#include <agx/lib/ca_layer.h>
#include <agx/lib/screen.h>

#include <libimg/libimg.h>

#include <libamc/libamc.h>

#include <stdlibadd/assert.h>
#include <stdlibadd/array.h>

#include <preferences/preferences_messages.h>
#include <file_manager/file_manager_messages.h>
#include <kb_driver/kb_driver_messages.h>

#include "awm.h"
#include "math.h"

typedef struct view {
	Rect frame;
	ca_layer* layer;
} view_t;

typedef struct user_window {
	Rect frame;
	ca_layer* layer;
	//view_t* title_view;
	view_t* content_view;
	const char* owner_service;
	const char* title;
	Rect close_button_frame;
	bool has_done_first_draw;
} user_window_t;

typedef struct incremental_mouse_state {
	int8_t state;
	int32_t rel_x;
	int32_t rel_y;
	int32_t rel_z;
	uint32_t combined_msg_count;
} incremental_mouse_state_t;

#define WINDOW_BORDER_MARGIN 0
#define WINDOW_TITLE_BAR_HEIGHT 30
#define WINDOW_TITLE_BAR_VISIBLE_HEIGHT (WINDOW_TITLE_BAR_HEIGHT - 2)

// Sorted by Z-index
#define MAX_WINDOW_COUNT 64
array_t* windows = NULL;

static Point mouse_pos = {0};

static user_window_t* _window_move_to_top(user_window_t* window);
static void _window_resize(user_window_t* window, Size new_size, bool inform_owner);
static void _write_window_title(user_window_t* window);
static void _redraw_window_title_bar(user_window_t* window, bool prospective_close_action);

Screen _screen = {0};

ca_layer* _g_background = NULL;
image_bmp_t* _g_title_bar_image = NULL;
image_bmp_t* _g_title_bar_x_unfilled = NULL;
image_bmp_t* _g_title_bar_x_filled = NULL;

Screen* gfx_screen() {
	if (_screen.physbase > 0) return &_screen;
	return NULL;
}

void write_screen(Screen* screen) {
    //vsync();
    uint8_t* raw_double_buf = screen->vmem->raw;
    memcpy(screen->physbase, screen->vmem->raw, screen->resolution.width * screen->resolution.height * screen->bytes_per_pixel);
}

static void handle_keystroke(amc_message_t* keystroke_msg) {
	key_event_t* event = (key_event_t*)keystroke_msg->body;

	if (event->type == KEY_PRESSED) {
		// Hack: Tab switches windows
		if (event->key == '\t') {
			_window_move_to_top(array_lookup(windows, windows->size - 1));
		}
	}

	// Only send this keystroke to the foremost program
	if (windows->size) {
		user_window_t* active_window = array_lookup(windows, 0);
		uint32_t awm_event = event->type == KEY_PRESSED ? AWM_KEY_DOWN : AWM_KEY_UP;
		amc_msg_u32_2__send(active_window->owner_service, awm_event, event->key);
	}
}

static user_window_t* _window_move_to_top(user_window_t* window) {
	uint32_t idx = array_index(windows, window);
	array_remove(windows, idx);

	// TODO(PT): array_insert_at_index
	array_t* a = windows;
	for (int32_t i = a->size; i >= 0; i--) {
		a->array[i] = a->array[i - 1];
	}
	a->array[0] = window;
	a->size += 1;
	return window;
}

typedef enum window_resize_edge {
	WINDOW_RESIZE_EDGE_LEFT = 0,
	WINDOW_RESIZE_EDGE_NOT_LEFT = 1
} window_resize_edge_t;

typedef struct mouse_interaction_state {
	bool left_click_down;
	user_window_t* active_window;

	// Drag state
	bool has_begun_drag;
	bool is_resizing_top_window;
	bool is_moving_top_window;
	window_resize_edge_t resize_edge;

	bool is_prospective_window_move;
	bool is_prospective_window_resize;
	Point mouse_pos;
} mouse_interaction_state_t;

static mouse_interaction_state_t g_mouse_state = {0};

static void _begin_left_click(mouse_interaction_state_t* state, Point mouse_point) {
	state->left_click_down = true;

	if (!state->active_window) {
		printf("Left click on background\n");
		return;
	}
	// Left click within a window
	// Set it to the topmost window if it wasn't already
	if (state->active_window != array_lookup(windows, 0)) {
		printf("Left click in background window %s. Move to top\n", state->active_window->owner_service);
		state->active_window = _window_move_to_top(state->active_window);
	}
	else {
		printf("Left click on topmost window %s\n", state->active_window->owner_service);
	}

	// Send the click event to the window
	Point local_mouse = point_make(
		mouse_point.x - state->active_window->frame.origin.x,
		mouse_point.y - state->active_window->frame.origin.y
	);
	local_mouse.x -= state->active_window->content_view->frame.origin.x;
	local_mouse.y -= state->active_window->content_view->frame.origin.y;
	awm_mouse_left_click_msg_t msg = {0};
	msg.event = AWM_MOUSE_LEFT_CLICK;
	msg.click_point = local_mouse;
	amc_message_construct_and_send(state->active_window->owner_service, &msg, sizeof(msg));
}

static void _end_left_click(mouse_interaction_state_t* state, Point mouse_point) {
	printf("End left click\n");
	state->left_click_down = false;
	if (state->active_window) {
		amc_msg_u32_3__send(state->active_window->owner_service, AWM_MOUSE_LEFT_CLICK_ENDED, mouse_point.x, mouse_point.y);
	}
}

static void _exit_hover_window(mouse_interaction_state_t* state) {
	printf("Mouse exited %s\n", state->active_window->owner_service);
	amc_msg_u32_1__send(state->active_window->owner_service, AWM_MOUSE_EXITED);
	state->active_window = NULL;
}

static void _enter_hover_window(mouse_interaction_state_t* state, user_window_t* window) {
	// Inform the window the mouse has just entered it
	amc_msg_u32_1__send(window->owner_service, AWM_MOUSE_ENTERED);
	// Keep track that we're currently within this window
	printf("Mouse entered %s\n", window->owner_service);
	state->active_window = window;
}

static void _mouse_reset_prospective_action_flags(mouse_interaction_state_t* state) {
	// Reset parameters about what actions the mouse may take based on its current position
	state->is_prospective_window_move = false;
	state->is_prospective_window_resize = false;
	if (state->active_window != NULL) {
		Point local_mouse = point_make(
			state->mouse_pos.x - state->active_window->frame.origin.x,
			state->mouse_pos.y - state->active_window->frame.origin.y
		);
		Rect content_view = state->active_window->content_view->frame;
		int inset = 8;
		Rect content_view_inset = rect_make(
			point_make(
				content_view.origin.x + inset,
				content_view.origin.y + inset
			),
			size_make(
				content_view.size.width - (inset * 2),
				content_view.size.height - (inset * 2)
			)
		);

		Rect title_text_box = rect_make(
			point_zero(),
			size_make(state->active_window->frame.size.width, WINDOW_TITLE_BAR_HEIGHT)
		);
		if (rect_contains_point(title_text_box, local_mouse)) {
			state->is_prospective_window_move = true;
		}
		else if (!rect_contains_point(content_view_inset, local_mouse)) {
			state->is_prospective_window_resize = true;
		}

		if (rect_contains_point(title_text_box, local_mouse)) {
			// TODO(PT): Keep track of what we last drew and only redraw if this is a state change
			_redraw_window_title_bar(state->active_window, true);
		}
		else {
			_redraw_window_title_bar(state->active_window, false);
		}
	}
}

static void _moved_in_hover_window(mouse_interaction_state_t* state, Point mouse_point) {
	if (!state->is_prospective_window_move && !state->is_prospective_window_resize) {
		Point local_mouse = point_make(
			mouse_point.x - state->active_window->frame.origin.x,
			mouse_point.y - state->active_window->frame.origin.y
		);
		// Mouse is hovered within the content view
		local_mouse.x -= state->active_window->content_view->frame.origin.x;
		local_mouse.y -= state->active_window->content_view->frame.origin.y;
		amc_msg_u32_3__send(state->active_window->owner_service, AWM_MOUSE_MOVED, local_mouse.x, local_mouse.y);
	}
}

static void _handle_mouse_moved(mouse_interaction_state_t* state, Point mouse_point, int32_t delta_x, int32_t delta_y, int32_t delta_z) {
	// Check if we've moved outside the bounds of the hover window
	if (state->active_window != NULL) {
		if (!rect_contains_point(state->active_window->frame, mouse_point)) {
			_exit_hover_window(state);
		}
	}

	// Check each window and see if we've just entered it
	// This array is sorted in Z-order so we encounter the topmost window first
	for (int i = 0; i < windows->size; i++) {
		user_window_t* window = array_lookup(windows, i);
		if (rect_contains_point(window->frame, mouse_point)) {
			// Is this a different window from the one we were previously hovered over?
			if (state->active_window != window) {
				// Inform the previous hover window that the mouse has exited it
				if (state->active_window != NULL) {
					_exit_hover_window(state);
				}
				_enter_hover_window(state, window);
			}
			break;
		}
	}

	// If we're still within a hover window, inform it that the mouse has moved
	if (state->active_window) {
		_moved_in_hover_window(state, mouse_point);
	}
}

static void _begin_mouse_drag(mouse_interaction_state_t* state, Point mouse_point) {
	state->has_begun_drag = true;
	if (state->active_window == NULL) {
		printf("Start drag on desktop background\n");
		return;
	}

	Point local_mouse = point_make(
		mouse_point.x - state->active_window->frame.origin.x,
		mouse_point.y - state->active_window->frame.origin.y
	);
	if (state->is_prospective_window_move) {
		state->is_moving_top_window = true;
		if (rect_contains_point(state->active_window->close_button_frame, local_mouse)) {
			amc_msg_u32_1__send(state->active_window->owner_service, AWM_CLOSE_WINDOW_REQUEST);
		}
	}
	else if (state->is_prospective_window_resize) {
		state->is_resizing_top_window = true;
		// Which side of the window are we dragging on?
		if (local_mouse.x < 10) {
			state->resize_edge = WINDOW_RESIZE_EDGE_LEFT;
		}
		else {
			state->resize_edge = WINDOW_RESIZE_EDGE_NOT_LEFT;
		}
	}
	else {
		printf("Start drag within content view\n");
	}
}

static void _end_mouse_drag(mouse_interaction_state_t* state, Point mouse_point) {
	if (state->has_begun_drag) {
		printf("End drag\n");
		state->has_begun_drag = false;
		state->is_moving_top_window = false;
		state->is_resizing_top_window = false;
	}
}

static void _adjust_window_position(user_window_t* window, int32_t delta_x, int32_t delta_y) {
	window->frame.origin.x += delta_x;
	window->frame.origin.y += delta_y;

	// Don't let the window go off-screen
	window->frame.origin.x = max(window->frame.origin.x, 0);
	window->frame.origin.y = max(window->frame.origin.y, 0);

	if (window->frame.origin.x + window->frame.size.width >= _screen.resolution.width) {
		uint32_t overhang = window->frame.origin.x + window->frame.size.width - _screen.resolution.width;
		window->frame.origin.x -= overhang;
	}
	if (window->frame.origin.y + window->frame.size.height >= _screen.resolution.height) {
		uint32_t overhang = window->frame.origin.y + window->frame.size.height - _screen.resolution.height;
		window->frame.origin.y -= overhang;
	}
}

static void _handle_mouse_dragged(mouse_interaction_state_t* state, Point mouse_point, int32_t delta_x, int32_t delta_y, int32_t delta_z) {
	// Nothing to do if we didn't start the drag with a hover window
	if (!state->active_window) {
		printf("Drag outside a hover window\n");
		return;
	}
	
	if (state->is_moving_top_window) {
		_adjust_window_position(state->active_window, delta_x, delta_y);
	}
	else if (state->is_resizing_top_window) {
		Size new_size = state->active_window->frame.size;

		// Dragging left from the left edge?
		if (state->resize_edge == WINDOW_RESIZE_EDGE_LEFT) {
			if (delta_x > 0) {
				new_size.width += delta_x;
			}
			else {
				// If the window is dragged to the left from the left edge,
				// move its origin and make it bigger
				new_size.width += -delta_x;
				_adjust_window_position(state->active_window, delta_x, 0);
			}

			if (delta_y > 0) {
				new_size.height += delta_y;
			}
			else {
				// If the window is dragged upwards from the left edge,
				// move its origin and make it bigger
				new_size.height += -delta_y;
				_adjust_window_position(state->active_window, 0, delta_y);
			}
		}
		else {
			new_size.width += delta_x;
			new_size.height += delta_y;
		}

		// Don't let the window get too small
		new_size.width = max(new_size.width, (WINDOW_BORDER_MARGIN * 2) + 20);
		new_size.height = max(new_size.height, WINDOW_TITLE_BAR_HEIGHT + (WINDOW_BORDER_MARGIN * 2) + 20);
		// Or too big...
		new_size.width = min(new_size.width, state->active_window->layer->size.width);
		new_size.height = min(new_size.height, state->active_window->layer->size.height);

		_window_resize(state->active_window, new_size, true);
	}
	else {
		// Drag within content view
		// Mouse is hovered within the content view
		Point local_mouse = point_make(
			mouse_point.x - state->active_window->frame.origin.x,
			mouse_point.y - state->active_window->frame.origin.y
		);
		local_mouse.x -= state->active_window->content_view->frame.origin.x;
		local_mouse.y -= state->active_window->content_view->frame.origin.y;
		amc_msg_u32_3__send(state->active_window->owner_service, AWM_MOUSE_DRAGGED, local_mouse.x, local_mouse.y);
	}
}

static void _handle_mouse_scroll(mouse_interaction_state_t* state, int32_t delta_z) {
	if (state->active_window) {
		// Scroll within a window
		// Inform the window
		amc_msg_u32_2__send(state->active_window->owner_service, AWM_MOUSE_SCROLLED, (uint32_t)delta_z);
	}
}

static void mouse_dispatch_events(uint8_t status_byte, Point mouse_point, int32_t delta_x, int32_t delta_y, int32_t delta_z) {
	mouse_interaction_state_t* mouse_state = &g_mouse_state;
	mouse_state->mouse_pos = mouse_point;

	// Is the left button clicked?
	if (status_byte & (1 << 0)) {
		// Were we already tracking a left-click?
		if (!mouse_state->left_click_down) {
			_begin_left_click(mouse_state, mouse_point);
			_begin_mouse_drag(mouse_state, mouse_point);
		}
		else {
			// The left mouse button is still held down - we're in a drag
			_handle_mouse_dragged(mouse_state, mouse_point, delta_x, delta_y, delta_z);
			return;
		}
	}
	else {
		// Did we just release a left click?
		if (mouse_state->left_click_down) {
			_end_left_click(mouse_state, mouse_point);
			_end_mouse_drag(mouse_state, mouse_point);
		}
	}

	// Did we scroll?
	if (delta_z != 0) {
		_handle_mouse_scroll(mouse_state, delta_z);
	}

	// Now that all other event types have been processed, check to see if we've just
	// moved to a different window
	_handle_mouse_moved(mouse_state, mouse_point, delta_x, delta_y, delta_z);
}

static bool handle_mouse_event(amc_message_t* mouse_event, incremental_mouse_state_t* incremental_update) {
	int8_t state = mouse_event->body[0];
	int8_t rel_x = mouse_event->body[1];
	int8_t rel_y = mouse_event->body[2];
	int8_t rel_z = mouse_event->body[3];
	//printf("awm received mouse packet (state %d) (delta %d %d %d)\n", state, rel_x, rel_y, rel_z);

	mouse_pos.x += rel_x;
	mouse_pos.y += rel_y;
	// Bind mouse to screen dimensions
	mouse_pos.x = max(0, mouse_pos.x);
	mouse_pos.y = max(0, mouse_pos.y);
	mouse_pos.x = min(_screen.resolution.width - 20, mouse_pos.x);
	mouse_pos.y = min(_screen.resolution.height - 20, mouse_pos.y);

	bool updated_state_byte = state == incremental_update->state;
	incremental_update->state = state;
	incremental_update->rel_x += rel_x;
	incremental_update->rel_y += rel_y;
	incremental_update->rel_z += rel_z;
	incremental_update->combined_msg_count += 1;
	return !updated_state_byte;
}

static void _draw_cursor(void) {
	mouse_interaction_state_t* mouse_state = &g_mouse_state;
	Color mouse_color = color_green();
	bool is_resize = false;
	if (mouse_state->is_resizing_top_window) {
		mouse_color = color_make(207, 25, 185);
		is_resize = true;
	}
	else if (mouse_state->is_moving_top_window) {
		mouse_color = color_make(30, 65, 217);
	}
	else if (mouse_state->is_prospective_window_resize) {
		mouse_color = color_make(212, 119, 201);
		is_resize = true;
	}
	else if (mouse_state->is_prospective_window_move) {
		mouse_color = color_make(121, 160, 217);
	}


	// Draw the new cursor
	Size cursor_size = size_make(14, 14);
	Rect new_mouse_rect = rect_make(mouse_pos, cursor_size);
	draw_rect(_screen.vmem, new_mouse_rect, color_black(), THICKNESS_FILLED);
	draw_rect(
		_screen.vmem, 
		rect_make(
			point_make(new_mouse_rect.origin.x + 2, new_mouse_rect.origin.y + 2), 
			size_make(10, 10)
		), 
		mouse_color, 
		THICKNESS_FILLED
	);

	// TODO(PT): Determine dirty region and combine these two rects
	//blit_layer(&dummy_layer, _screen.vmem, old_mouse_rect, old_mouse_rect);
	//blit_layer(&dummy_layer, _screen.vmem, new_mouse_rect, new_mouse_rect);
}

static user_window_t* _window_for_service(const char* owner_service) {
	for (int i = 0; i < windows->size; i++) {
		user_window_t* window = array_lookup(windows, i);
		if (!strcmp(window->owner_service, owner_service)) {
			return window;
		}
	}
	printf("No window for service: %s\n", owner_service);
	return NULL;
}

static int32_t _window_idx_for_service(const char* owner_service) {
	for (int i = 0; i < windows->size; i++) {
		user_window_t* window = array_lookup(windows, i);
		if (!strcmp(window->owner_service, owner_service)) {
			return i;
		}
	}
	return -1;
}

static void _redraw_window_title_bar(user_window_t* window, bool prospective_close_action) {
	if (!_g_title_bar_image) {
		printf("No images yet...\n");
		return;
	}

	Size title_bar_size = size_make(window->frame.size.width, WINDOW_TITLE_BAR_HEIGHT);
	image_render_to_layer(
		_g_title_bar_image, 
		window->layer, 
		rect_make(
			point_zero(), 
			size_make(title_bar_size.width, WINDOW_TITLE_BAR_VISIBLE_HEIGHT)
		)
	);

	//bool is_x_filled = g_mouse_state.active_window == window && (g_mouse_state.is_prospective_window_move || g_mouse_state.is_moving_top_window);
	image_bmp_t* x_image = (prospective_close_action) ? _g_title_bar_x_filled : _g_title_bar_x_unfilled;
	uint32_t icon_height = x_image->size.height;
	window->close_button_frame = rect_make(
		point_make(icon_height * 0.75, icon_height * 0.275), 
		x_image->size
	);
	image_render_to_layer(
		x_image, 
		window->layer, 
		window->close_button_frame
	);

	// Draw window title
	_write_window_title(window);
}

static void _window_resize(user_window_t* window, Size new_size, bool inform_window) {
	//printf("window_resize[%s] = (%d, %d)\n", window->owner_service, new_size.width, new_size.height);
	window->frame.size = new_size;
	//draw_rect(window->layer, rect_make(point_zero(), window->frame.size), color_black(), THICKNESS_FILLED);

	// Resize the text box
	Size title_bar_size = size_make(new_size.width, WINDOW_TITLE_BAR_HEIGHT);

	// The content view is a bit smaller to accomodate decorations
	window->content_view->frame = rect_make(
		point_make(
			WINDOW_BORDER_MARGIN, 
			// The top edge does not have a margin
			title_bar_size.height
		), 
		size_make(
			new_size.width - (WINDOW_BORDER_MARGIN * 2),
			// No need to multiply margin by 2 since the top edge doesn't have a margin
			new_size.height - WINDOW_BORDER_MARGIN - title_bar_size.height
		)
	);

	_redraw_window_title_bar(window, false);

	if (inform_window) {
		awm_window_resized_msg_t msg = {0};
		msg.event = AWM_WINDOW_RESIZED;
		msg.new_size = window->content_view->frame.size;
		amc_message_construct_and_send(window->owner_service, &msg, sizeof(msg));
	}
}

static void window_create(const char* owner_service, uint32_t width, uint32_t height) {
	user_window_t* window = calloc(1, sizeof(user_window_t));
	array_insert(windows, window);

	// Shared layer is size of the screen to allow window resizing
	uint32_t shmem_size = _screen.resolution.width * _screen.resolution.height * _screen.bytes_per_pixel; 
	uint32_t shmem_local = 0;
	uint32_t shmem_remote = 0;

	printf("Creating framebuffer for %s\n", owner_service);
	uint32_t local_buffer;
	uint32_t remote_buffer;
	amc_shared_memory_create(
		owner_service, 
		shmem_size, 
		&shmem_local, 
		&shmem_remote
	);

	// Place the window in the center of the screen
	Point origin = point_make(
		(_screen.resolution.width / 2) - (width / 2),
		(_screen.resolution.height / 2) - (height / 2)
	);
	window->frame = rect_make(origin, size_zero());
	window->layer = create_layer(_screen.resolution);

	view_t* content_view = calloc(1, sizeof(view_t));
	content_view->layer = calloc(1, sizeof(ca_layer));
	content_view->layer->size = _screen.resolution;
	content_view->layer->raw = (uint8_t*)shmem_local;
	content_view->layer->alpha = 1.0;
	window->content_view = content_view;

	// Copy the owner service name as we don't own it
	window->owner_service = strndup(owner_service, AMC_MAX_SERVICE_NAME_LEN);

	// Configure the title text box
	// The size will be reset by window_size()
	window->title = strndup(window->owner_service, strlen(window->owner_service));
	_write_window_title(window);

	// Make the window a bit bigger than the user requested to accomodate for decorations
	int full_window_width = width + (WINDOW_BORDER_MARGIN * 2);
	Size title_bar_size = size_make(full_window_width, WINDOW_TITLE_BAR_HEIGHT);
	Size full_window_size = size_make(
		full_window_width, 
		// We only need to add the border margin on the bottom edge
		// The top edge does not have a border margin
		height + title_bar_size.height + WINDOW_BORDER_MARGIN
	);

	// Make the new window show up on top
	window = _window_move_to_top(window);

	// Now that we've configured the initial window state on our end, 
	// provide the buffer to the client
	printf("AWM made shared framebuffer for %s\n", owner_service);
	printf("\tAWM    memory: 0x%08x - 0x%08x\n", shmem_local, shmem_local + shmem_size);
	printf("\tRemote memory: 0x%08x - 0x%08x\n", shmem_remote, shmem_remote + shmem_size);
	amc_msg_u32_2__send(owner_service, AWM_CREATED_WINDOW_FRAMEBUFFER, shmem_remote);
	// Inform the window of its initial size
	_window_resize(window, full_window_size, true);
}

static void _window_destroy(user_window_t* window) {
	layer_teardown(window->layer);

	// Special 'virtual' layer that doesn't need its internal buffer freed (because it's backed by shared memory)
	free(window->content_view->layer);
	free(window->content_view);

	free(window->owner_service);
	free(window->title);

	free(window);
}

static void _update_window_framebuf_idx(int idx) {
	user_window_t* window = array_lookup(windows, idx);
	//blit_layer(_screen.vmem, &window->shared_layer, window->frame, rect_make(point_zero(), window->frame.size));
	//blit_layer(_screen.vmem, window->layer, window->frame, rect_make(point_zero(), window->frame.size));
	blit_layer(window->layer, window->content_view->layer, window->content_view->frame, rect_make(point_zero(), window->content_view->frame.size));
	//blit_layer(_screen.vmem, window->layer, window->frame, rect_make(point_zero(), window->frame.size));
}

static void _update_window_framebuf(const char* owner_service) {
	user_window_t* window = _window_for_service(owner_service);
	if (!window) {
		printf("Failed to find a window for %s\n", owner_service);
		return;
	}
	window->has_done_first_draw = true;

	blit_layer(
		window->layer, 
		window->content_view->layer, 
		window->content_view->frame, 
		rect_make(point_zero(), window->content_view->frame.size)
	);
}

static void _write_window_title(user_window_t* window) {
	Point mid = point_make(
		window->frame.size.width / 2.0, 
		(WINDOW_TITLE_BAR_VISIBLE_HEIGHT) / 2.0
	);
	Size font_size = size_make(8, 12);
	uint32_t len = strlen(window->title);
	Point origin = point_make(
		mid.x - ((font_size.width * len) / 2.0),
		mid.y - (font_size.height / 2.0) - 1
	);
	for (uint32_t i = 0; i < len; i++) {
		draw_char(
			window->layer,
			window->title[i],
			origin.x + (font_size.width * i),
			origin.y,
			color_make(50, 50, 50),
			font_size
		);
	}
}

static void _update_window_title(const char* owner_service, awm_window_title_msg_t* title_msg) {
	printf("update_window_title %s %s\n", owner_service, title_msg->title);
	user_window_t* window = _window_for_service(owner_service);
	if (!window) {
		printf("Failed to find a window for %s\n", owner_service);
		return;
	}

	if (window->title) {
		free(window->title);
	}
	window->title = strndup(title_msg->title, title_msg->len);
	_window_resize(window, window->frame.size, false);
}

void _radial_gradiant(ca_layer* layer, Size gradient_size, Color c1, Color c2, int x1, int y1, float r);

static void handle_user_message(amc_message_t* user_message, array_t* windows_to_update) {
	const char* source_service = amc_message_source(user_message);
	uint32_t command = amc_msg_u32_get_word(user_message, 0);

	if (!strncmp(source_service, PREFERENCES_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
		if (command == AWM_PREFERENCES_UPDATED) {
			prefs_updated_msg_t* msg = (prefs_updated_msg_t*)&user_message->body;
			printf("AWM updating background gradient... %d %d %d, %d %d %d\n",msg->from.val[0], msg->from.val[1], msg->from.val[2], msg->to.val[0], msg->to.val[1], msg->to.val[2]);
			_radial_gradiant(
				_g_background, 
				_g_background->size, 
				msg->from,
				msg->to,
				_g_background->size.width/2.0, 
				_g_background->size.height/2.0, 
				(float)_g_background->size.height * 1.3
			);
			return;
		}
	}
	// User requesting a window to draw in to?
	if (command == AWM_REQUEST_WINDOW_FRAMEBUFFER) {
		uint32_t width = amc_msg_u32_get_word(user_message, 1);
		uint32_t height = amc_msg_u32_get_word(user_message, 2);
		window_create(source_service, width, height);
		return;
	}
	else if (command == AWM_WINDOW_REDRAW_READY) {
		user_window_t* window = _window_for_service(source_service);
		if (array_index(windows_to_update, window) == -1) {
			printf("Ready for redraw: %s\n", source_service);
			array_insert(windows_to_update, window);
		}
	}
	else if (command == AWM_UPDATE_WINDOW_TITLE) {
		awm_window_title_msg_t* title_msg =  (awm_window_title_msg_t*)user_message->body;
		_update_window_title(source_service, title_msg);
	}
	else if (command == AWM_CLOSE_WINDOW) {
		user_window_t* window = _window_for_service(source_service);
		uint32_t i = array_index(windows, window);
		array_remove(windows, i);
		_window_destroy(window);
		if (g_mouse_state.active_window == window) {
			printf("Clear active window 0x%08x\n", window);
			g_mouse_state.active_window = NULL;
		}
	}
	else {
		printf("Unknown message from %s: %d\n", source_service, command);
	}
}

Color transcolor(Color c1, Color c2, float d) {
	if (d < 0) d = 0;
	if (d > 1) d = 1;
	return color_make(
		(c1.val[0] * (1 - d)) + (c2.val[0] * d),
		(c1.val[1] * (1 - d)) + (c2.val[1] * d),
		(c1.val[2] * (1 - d)) + (c2.val[2] * d)
	);
}

float pifdist(int x1, int y1, int x2, int y2) {
	float x = x1 - x2;
	float y = y1 - y2;
	return sqrt(x * x + y * y);
}

void _radial_gradiant(ca_layer* layer, Size gradient_size, Color c1, Color c2, int x1, int y1, float r) {
	int x_step = gradient_size.width / 200.0;
	int y_step = gradient_size.height / 200.0;
    if (x_step < 1) x_step = 1;
    if (y_step < 1) y_step = 1;
	for (uint32_t y = 0; y < gradient_size.height; y += y_step) {
		for (uint32_t x = 0; x < gradient_size.width; x += x_step) {
			Color c = transcolor(c1, c2, pifdist(x1, y1, x, y) / r);
			for (int i = 0; i < x_step; i++) {
				for (int j = 0; j < y_step; j++) {
					putpixel(layer, x+i, y+j, c);
				}
			}
		}
	}
}

static image_bmp_t* _load_image(const char* image_name) {
	printf("AWM sending read file request for %s...\n", image_name);
	file_manager_read_file_request_t req = {0};
	req.event = FILE_MANAGER_READ_FILE;
	snprintf(req.path, sizeof(req.path), "%s", image_name);
	amc_message_construct_and_send(FILE_MANAGER_SERVICE_NAME, &req, sizeof(file_manager_read_file_request_t));

	printf("AWM awaiting file read response for %s...\n", image_name);
	amc_message_t* file_data_msg;
	bool received_file_data = false;
	for (uint32_t i = 0; i < 32; i++) {
		amc_message_await(FILE_MANAGER_SERVICE_NAME, &file_data_msg);
		uint32_t event = amc_msg_u32_get_word(file_data_msg, 0);
		if (event == FILE_MANAGER_READ_FILE_RESPONSE) {
			received_file_data = true;
			break;
		}
	}
	assert(received_file_data, "Failed to recv file data");

	printf("AWM got response for %s!\n", image_name);
	file_manager_read_file_response_t* resp = (file_manager_read_file_response_t*)&file_data_msg->body;
	uint8_t* b = &resp->file_data;

	return image_parse_bmp(resp->file_size, resp->file_data);
}

int main(int argc, char** argv) {
	amc_register_service("com.axle.awm");
	windows = array_create(MAX_WINDOW_COUNT);

	// Ask the kernel to map in the framebuffer and send us info about it
	amc_msg_u32_1__send(AXLE_CORE_SERVICE_NAME, AMC_AWM_MAP_FRAMEBUFFER);

	amc_message_t* msg;
	amc_message_await(AXLE_CORE_SERVICE_NAME, &msg);
	uint32_t event = amc_msg_u32_get_word(msg, 0);
	assert(event == AMC_AWM_MAP_FRAMEBUFFER_RESPONSE, "Expected awm framebuffer info");
	amc_framebuffer_info_t* framebuffer_info = (amc_framebuffer_info_t*)msg->body;
	printf("Recv'd framebuf info!\n");
    printf("0x%08x 0x%08x (%d x %d x %d x %d)\n", framebuffer_info->address, framebuffer_info->size, framebuffer_info->width, framebuffer_info->height, framebuffer_info->bytes_per_pixel, framebuffer_info->bits_per_pixel);

    _screen.physbase = (uint32_t*)framebuffer_info->address;
    _screen.video_memory_size = framebuffer_info->size;

    _screen.resolution = size_make(framebuffer_info->width, framebuffer_info->height);
    _screen.bits_per_pixel = framebuffer_info->bits_per_pixel;
    _screen.bytes_per_pixel = framebuffer_info->bytes_per_pixel;

    // Font size is calculated as a fraction of screen size
    //_screen.default_font_size = font_size_for_resolution(_screen.resolution);
    //_screen.default_font_size = size_make(16, 16);

	Rect screen_frame = rect_make(point_zero(), _screen.resolution);
	_g_background = create_layer(screen_frame.size);
	_radial_gradiant(
		_g_background, 
		_g_background->size, 
		color_make(39, 67, 255), 
		color_make(2, 184, 255),
		_g_background->size.width/2.0, 
		_g_background->size.height/2.0, 
		(float)_g_background->size.height * 1.3
	);
    _screen.vmem = create_layer(screen_frame.size);

    printf("awm graphics: %d x %d, %d BPP @ 0x%08x\n", _screen.resolution.width, _screen.resolution.height, _screen.bits_per_pixel, _screen.physbase);

	ca_layer dummy_layer;
	dummy_layer.size = _screen.resolution;
	dummy_layer.raw = (uint8_t*)_screen.physbase;
	dummy_layer.alpha = 1.0;

	// Draw the background onto the screen buffer to start off
	blit_layer(_screen.vmem, _g_background, screen_frame, screen_frame);
	blit_layer(&dummy_layer, _screen.vmem, screen_frame, screen_frame);

	array_t* windows_to_update = array_create(MAX_WINDOW_COUNT);
	while (true) {
		// Wait for a system event or window event
		amc_message_t* msg;
		incremental_mouse_state_t incremental_mouse_update = {0};
		memset(&incremental_mouse_update, 0, sizeof(incremental_mouse_state_t));
		incremental_mouse_update.combined_msg_count = 0;
		do {
			// Wait until we've unblocked with at least one message available
			amc_message_await_any(&msg);
			// Will automatically respond to watchdog pings
			if (libamc_handle_message(msg)) {
				continue;
			}
			const char* source_service = amc_message_source(msg);

			// Always update the prospective mouse action flags when the event loop runs
			_mouse_reset_prospective_action_flags(&g_mouse_state);

			// Process the message we just received
			if (!strcmp(source_service, KB_DRIVER_SERVICE_NAME)) {
				handle_keystroke(msg);
				// Skip redrawing for now - the above will send a KB call to the
				// foremost program, which will later redraw its window with the new info
				continue;
			}
			else if (!strcmp(source_service, "com.axle.mouse_driver")) {
				// Update the mouse position based on the data packet
				bool changed_state = handle_mouse_event(msg, &incremental_mouse_update);
				if (changed_state) {
					/*
					if (incremental_mouse_update.combined_msg_count) {
						printf("Dispatch mouse, deduplicated %d\n", incremental_mouse_update.combined_msg_count);
					}
					*/
					mouse_dispatch_events(
						incremental_mouse_update.state, 
						mouse_pos, 
						incremental_mouse_update.rel_x,
						incremental_mouse_update.rel_y,
						incremental_mouse_update.rel_z
					);
					memset(&incremental_mouse_update, 0, sizeof(incremental_mouse_state_t));
					incremental_mouse_update.combined_msg_count = 0;
				}
			}
			else {
				// TODO(PT): If a window sends REDRAW_READY, we can put it onto a "ready to redraw" list
				// Items can be popped off the list based on their Z-index, or a periodic time-based update
				// TODO(PT): If a window has requested multiple redraws within a single awm event loop
				// pass, awm should redraw it only once
				handle_user_message(msg, windows_to_update);
			}
		} while (amc_has_message());

		while (windows_to_update->size) {
			user_window_t* window_to_update = array_lookup(windows_to_update, 0);
			_update_window_framebuf(window_to_update->owner_service);
			array_remove(windows_to_update, 0);
		}

		// We're out of messages to process - composite everything together and redraw
		// First draw the background
		blit_layer(_screen.vmem, _g_background, screen_frame, screen_frame);
		// Then each window (without copying in the window's current shared framebuffer)
		// Draw the bottom-most windows first
		// TODO(PT): Replace with a loop that draws the topmost window and 
		// splits lower windows into visible regions, then blits those
		for (int i = windows->size - 1; i >= 0; i--) {
			user_window_t* window = array_lookup(windows, i);
			if (!window->has_done_first_draw) {
				continue;
			}
			// As an optimization until we have visible-region splitting, skip drawing 
			// fully occluded windows
			bool fully_occluded = false;
			for (int j = i-1; j >= 0; j--) {
				user_window_t* higher_window = array_lookup(windows, j);
				if (higher_window->layer->alpha < 1.0) {
					// This optimization doesn't apply with transparent windows
					continue;
				}
				Rect higher_frame = higher_window->frame;
				Rect lower_frame = window->frame;
				if (rect_min_x(higher_frame) <= rect_min_x(lower_frame) &&
					rect_min_y(higher_frame) <= rect_min_y(lower_frame) &&
					rect_max_x(higher_frame) >= rect_max_x(lower_frame) &&
					rect_max_y(higher_frame) >= rect_max_y(lower_frame)) {
					fully_occluded = true;
					break;
				}
			}
			if (!fully_occluded) {
				// TODO(PT): As per the above comment, we should only copy the window layer
				// once if it's requested a redraw at least once on this pass through the event loop
				blit_layer(
					_screen.vmem, 
					window->layer, 
					window->frame, 
					rect_make(
						point_zero(), 
						size_make(
							window->frame.size.width,
							WINDOW_TITLE_BAR_VISIBLE_HEIGHT
						)
					)
				);
				blit_layer(
					_screen.vmem, 
					window->layer, 
					rect_make(
						point_make(
							window->frame.origin.x,
							window->frame.origin.y + WINDOW_TITLE_BAR_HEIGHT
						),
						size_make(
							window->frame.size.width,
							window->frame.size.height - WINDOW_TITLE_BAR_HEIGHT
						)
					),
					rect_make(
						point_make(
							0,
							WINDOW_TITLE_BAR_HEIGHT
						), 
						size_make(
							window->frame.size.width,
							window->frame.size.height - WINDOW_TITLE_BAR_HEIGHT
						)
					)
				);
			}
		}

		// And finally the cursor
		_draw_cursor();

		// Copy our internal screen buffer to video memory
		memcpy(_screen.physbase, _screen.vmem, _screen.resolution.width * _screen.resolution.height * _screen.bytes_per_pixel);
			if (!_g_title_bar_image) {
				_g_title_bar_image = _load_image("titlebar7.bmp");
				_g_title_bar_x_filled = _load_image("titlebar_x_filled2.bmp");
				_g_title_bar_x_unfilled = _load_image("titlebar_x_unfilled2.bmp");

				for (uint32_t i = 0; i < windows->size; i++) {
					user_window_t* w = array_lookup(windows, i);
					_window_resize(w, w->frame.size, false);
				}
			}
	}
	return 0;
}
