#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>

#include <agx/font/font.h>
#include <agx/lib/size.h>
#include <agx/lib/screen.h>
#include <agx/lib/shapes.h>
#include <agx/lib/ca_layer.h>
#include <agx/lib/putpixel.h>
#include <agx/lib/text_box.h>

#include <libamc/libamc.h>

#include <stdlibadd/assert.h>

#include "awm.h"
#include "gfx.h"
#include "math.h"

typedef struct view {
	Rect frame;
	ca_layer* layer;
} view_t;

typedef struct user_window {
	Rect frame;
	text_box_t* title_text_box;
	ca_layer* layer;
	//view_t* title_view;
	view_t* content_view;
	const char* owner_service;
} user_window_t;

#define WINDOW_BORDER_MARGIN 4
#define WINDOW_TITLE_BAR_HEIGHT 28

// Sorted by Z-index
#define MAX_WINDOW_COUNT 64
user_window_t windows[MAX_WINDOW_COUNT] = {0};
int window_count = 0;

static Point mouse_pos = {0};

static user_window_t* _window_move_to_top(user_window_t* window);
static void _window_resize(user_window_t* window, Size new_size, bool inform_owner);
static void _write_window_title(user_window_t* window, uint32_t len, const char* title);

Screen _screen = {0};

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
	for (int i = 0; i < keystroke_msg->len; i++) {
		char ch = keystroke_msg->body[i];
		// Only send this keystroke to the foremost program
		if (window_count) {
			user_window_t* active_window = &windows[0];
			amc_msg_u32_2__send(active_window->owner_service, AWM_KEY_DOWN, ch);
		}

		// Hack: Tab switches windows
		if (ch == '\t') {
			_window_move_to_top(&windows[window_count-1]);
		}
	}
}

static user_window_t* _window_move_to_top(user_window_t* window) {
	user_window_t moved_window = *window;
	int win_pos = window - windows;
	// TODO(PT): replace with memmove
	for (int i = win_pos; i > 0; i--) {
		windows[i] = windows[i-1];
	}
	windows[0] = moved_window;
	return &windows[0];
}

typedef struct mouse_interaction_state {
	bool left_click_down;
	user_window_t* active_window;

	// Drag state
	bool has_begun_drag;
	bool is_resizing_top_window;
	bool is_moving_top_window;

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
	if (state->active_window != &windows[0]) {
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
	awm_mouse_left_click_msg_t msg = {0};
	msg.event = AWM_MOUSE_LEFT_CLICK;
	msg.click_point = local_mouse;
	amc_message_construct_and_send(state->active_window->owner_service, &msg, sizeof(msg));
}

static void _end_left_click(mouse_interaction_state_t* state, Point mouse_point) {
	printf("End left click\n");
	state->left_click_down = false;
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
		int inset = 4;
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
			state->active_window->title_text_box->size
		);
		if (rect_contains_point(title_text_box, local_mouse)) {
			state->is_prospective_window_move = true;
		}
		else if (!rect_contains_point(content_view_inset, local_mouse)) {
			state->is_prospective_window_resize = true;
		}
	}
}

static void _moved_in_hover_window(mouse_interaction_state_t* state, Point mouse_point) {
	Point local_mouse = point_make(
		mouse_point.x - state->active_window->frame.origin.x,
		mouse_point.y - state->active_window->frame.origin.y
	);

	if (!state->is_prospective_window_move && !state->is_prospective_window_resize) {
		// Mouse is hovered within the content view
		local_mouse.x -= state->active_window->content_view->frame.origin.x;
		local_mouse.y -= state->active_window->content_view->frame.origin.y;
		amc_msg_u32_3__send(state->active_window->owner_service, AWM_MOUSE_MOVED, local_mouse.x, local_mouse.y);
	}
}

static void _handle_mouse_moved(mouse_interaction_state_t* state, Point mouse_point, int8_t delta_x, int8_t delta_y, int8_t delta_z) {
	// Check if we've moved outside the bounds of the hover window
	if (state->active_window != NULL) {
		if (!rect_contains_point(state->active_window->frame, mouse_point)) {
			_exit_hover_window(state);
		}
	}

	// Check each window and see if we've just entered it
	// This array is sorted in Z-order so we encounter the topmost window first
	for (int i = 0; i < window_count; i++) {
		user_window_t* window = &windows[i];
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

	if (state->is_prospective_window_move) {
		state->is_moving_top_window = true;
	}
	else if (state->is_prospective_window_resize) {
		state->is_resizing_top_window = true;
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

static void _handle_mouse_dragged(mouse_interaction_state_t* state, Point mouse_point, int8_t delta_x, int8_t delta_y, int8_t delta_z) {
	// Nothing to do if we didn't start the drag with a hover window
	if (!state->active_window) {
		printf("Drag outside a hover window\n");
		return;
	}
	
	if (state->is_moving_top_window) {
		state->active_window->frame.origin.x += delta_x;
		state->active_window->frame.origin.y += delta_y;
	}
	else if (state->is_resizing_top_window) {
		Size new_size = state->active_window->frame.size;
		new_size.width += delta_x;
		new_size.height += delta_y;

		// Don't let the window get too small
		new_size.width = max(new_size.width, WINDOW_TITLE_BAR_HEIGHT + 2);
		new_size.height = max(new_size.height, WINDOW_TITLE_BAR_HEIGHT + 2);

		_window_resize(state->active_window, new_size, true);
	}
}

static void _handle_mouse_scroll(mouse_interaction_state_t* state, int8_t delta_z) {
	if (state->active_window) {
		// Scroll within a window
		// Inform the window
		amc_msg_u32_2__send(state->active_window->owner_service, AWM_MOUSE_SCROLLED, (uint32_t)delta_z);
	}
}

static void mouse_dispatch_events(uint8_t status_byte, Point mouse_point, int8_t delta_x, int8_t delta_y, int8_t delta_z) {
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

static void handle_mouse_event(amc_message_t* mouse_event) {
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

	mouse_dispatch_events(state, mouse_pos, rel_x, rel_y, rel_z);
}

static void _draw_cursor(void) {
	mouse_interaction_state_t* mouse_state = &g_mouse_state;
	Color mouse_color = color_green();
	if (mouse_state->is_resizing_top_window) {
		//mouse_color = color_make(217, 107, 32);
		mouse_color = color_make(207, 25, 185);
	}
	else if (mouse_state->is_moving_top_window) {
		mouse_color = color_make(30, 65, 217);
	}
	else if (mouse_state->is_prospective_window_resize) {
		//mouse_color = color_make(217, 148, 100);
		mouse_color = color_make(212, 119, 201);
	}
	else if (mouse_state->is_prospective_window_move) {
		mouse_color = color_make(121, 160, 217);
	}

	// Re-draw the background where the mouse has just left
	Size cursor_size = size_make(14, 14);
	//Rect old_mouse_rect = rect_make(old_mouse_pos, cursor_size);
	//blit_layer(_screen.vmem, background, old_mouse_rect, old_mouse_rect);

	// Draw the new cursor
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
	for (int i = 0; i < window_count; i++) {
		if (!strcmp(windows[i].owner_service, owner_service)) {
			return &windows[i];
		}
	}
	printf("No window for service: %s\n", owner_service);
	return NULL;
}

static int32_t _window_idx_for_service(const char* owner_service) {
	for (int i = 0; i < window_count; i++) {
		if (!strcmp(windows[i].owner_service, owner_service)) {
			return i;
		}
	}
	return -1;
}

static void _window_resize(user_window_t* window, Size new_size, bool inform_window) {
	//printf("window_resize[%s] = (%d, %d)\n", window->owner_service, new_size.width, new_size.height);
	window->frame.size = new_size;
	//draw_rect(window->layer, rect_make(point_zero(), window->frame.size), color_black(), THICKNESS_FILLED);

	// Resize the text box
	Size title_bar_size = size_make(new_size.width, WINDOW_TITLE_BAR_HEIGHT);
	//text_box_resize(window->title_text_box, title_bar_size);

	// The content view is a bit smaller to accomodate decorations
	window->content_view->frame = rect_make(
		point_make(
			WINDOW_BORDER_MARGIN, 
			title_bar_size.height + WINDOW_BORDER_MARGIN
		), 
		size_make(
			new_size.width - (WINDOW_BORDER_MARGIN * 2),
			new_size.height - (WINDOW_BORDER_MARGIN * 2) - title_bar_size.height
		)
	);

	// Draw top window bar
	Color c1 = color_make(70, 80, 130);
	Color c2 = color_make(50, 50, 50);
	Color* c = &c1;
	int block_size = 4;
	bool flip = false;
	for (int y = 0; y < title_bar_size.height; y++) {
		if (y % block_size == 0) flip = !flip;
		c = flip ? &c1 : &c2;
		for (int x = 0; x < title_bar_size.width; x++) {
			if (x % block_size == 0) {
				c = (c == &c1) ? &c2 : &c1;
			}
			putpixel(window->layer, x, y, *c);
		}
	}

	// Draw a bordering rectangle around the title bar
	draw_rect(window->layer,
			  rect_make(point_zero(), title_bar_size), 
			  color_black(), 
			  1);

	// Draw window title
	uint32_t title_len = ((window->title_text_box->font_size.width + window->title_text_box->font_padding.width) * strlen(window->owner_service));
	Point title_text_origin = point_make(WINDOW_BORDER_MARGIN, WINDOW_BORDER_MARGIN);

	Size visible_title_bar_size = size_make(title_len, 14);
	blit_layer(
		window->layer, 
		window->title_text_box->scroll_layer->layer, 
		rect_make(
			point_make(
				title_text_origin.x + 2,
				title_text_origin.y + 1
			),
			visible_title_bar_size
		),
		rect_make(point_zero(), visible_title_bar_size)
	);

	draw_rect(
		window->layer, 
		rect_make(
			point_make(0, WINDOW_TITLE_BAR_HEIGHT), 
			size_make(window->frame.size.width, window->frame.size.height - WINDOW_TITLE_BAR_HEIGHT)
		), 
		color_black(), 
		WINDOW_BORDER_MARGIN
	);

	awm_window_resized_msg_t msg = {0};
	msg.event = AWM_WINDOW_RESIZED;
	msg.new_size = new_size;
	amc_message_construct_and_send(window->owner_service, &msg, sizeof(msg));
}

static void window_create(const char* owner_service, uint32_t width, uint32_t height) {
	int window_idx = window_count++;
	if (window_count > sizeof(windows) / sizeof(windows[0])) {
		assert(0, "too many windows");
	}

	printf("Creating framebuffer for %s (window idx %d)\n", owner_service, window_idx);
	// TODO(PT): By always making the buffer the size of the screen, we allow for window resizing later
	uint32_t buffer_size = _screen.resolution.width * _screen.resolution.height * _screen.bytes_per_pixel;
	uint32_t local_buffer;
	uint32_t remote_buffer;
	amc_shared_memory_create(owner_service, buffer_size, &local_buffer, &remote_buffer);

	user_window_t* window = &windows[window_idx];
	// Place the window in the center of the screen
	Point origin = point_make(
		(_screen.resolution.width / 2) - (width / 2),
		(_screen.resolution.height / 2) - (height / 2)
	);
	window->frame = rect_make(origin, size_zero());
	window->layer = create_layer(_screen.resolution);

	view_t* content_view = malloc(sizeof(view_t));
	content_view->layer = malloc(sizeof(ca_layer));
	content_view->layer->size = _screen.resolution;
	content_view->layer->raw = (uint8_t*)local_buffer;
	content_view->layer->alpha = 1.0;
	window->content_view = content_view;

	// Copy the owner service name as we don't own it
	window->owner_service = strndup(owner_service, AMC_MAX_SERVICE_NAME_LEN);

	// Configure the title text box
	// The size will be reset by window_size()
	window->title_text_box = text_box_create__unscrollable(size_make(_screen.resolution.width, WINDOW_TITLE_BAR_HEIGHT*2), color_dark_gray());
	window->title_text_box->preserves_history = false;
	window->title_text_box->text_inset = point_make(0, 0);
	_write_window_title(window, strlen(window->owner_service), window->owner_service);

	// Make the window a bit bigger than the user requested to accomodate for decorations
	int full_window_width = width + (WINDOW_BORDER_MARGIN * 2);
	Size title_bar_size = size_make(full_window_width, WINDOW_TITLE_BAR_HEIGHT);
	Size full_window_size = size_make(
		full_window_width, 
		height + title_bar_size.height + (WINDOW_BORDER_MARGIN * 2)
	);
	_window_resize(window, full_window_size, false);

	// Make the new window show up on top
	window = _window_move_to_top(window);

	// Now that we've configured the initial window state on our end, 
	// provide the buffer to the client
	printf("AWM made shared framebuffer for %s\n", owner_service);
	printf("\tAWM    memory: 0x%08x - 0x%08x\n", local_buffer, local_buffer + buffer_size);
	printf("\tRemote memory: 0x%08x - 0x%08x\n", remote_buffer, remote_buffer + buffer_size);
	amc_msg_u32_2__send(owner_service, AWM_CREATED_WINDOW_FRAMEBUFFER, remote_buffer);
	_window_resize(window, full_window_size, true);
}

static void _update_window_framebuf_idx(int idx) {
	if (idx >= window_count) assert(0, "invalid index");
	user_window_t* window = &windows[idx];
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
	blit_layer(
		window->layer, 
		window->content_view->layer, 
		window->content_view->frame, 
		rect_make(point_zero(), window->content_view->frame.size)
	);
}

static void _write_window_title(user_window_t* window, uint32_t len, const char* title) {
	text_box_clear_and_erase_history(window->title_text_box);
	text_box_puts(window->title_text_box, title, color_white());
	// Perform a 'fake' resize to force the title bar to be redrawn
	_window_resize(window, window->frame.size, false);
}

static void _update_window_title(const char* owner_service, awm_window_title_msg_t* title_msg) {
	printf("update_window_title %s %s\n", owner_service, title_msg->title);
	user_window_t* window = _window_for_service(owner_service);
	if (!window) {
		printf("Failed to find a window for %s\n", owner_service);
		return;
	}
	_write_window_title(window, title_msg->len, title_msg->title);
}

static void handle_user_message(amc_message_t* user_message) {
	const char* source_service = amc_message_source(user_message);
	// User requesting a window to draw in to?
	uint32_t command = amc_msg_u32_get_word(user_message, 0);
	if (command == AWM_REQUEST_WINDOW_FRAMEBUFFER) {
		uint32_t width = amc_msg_u32_get_word(user_message, 1);
		uint32_t height = amc_msg_u32_get_word(user_message, 2);
		window_create(source_service, width, height);
		return;
	}
	else if (command == AWM_WINDOW_REDRAW_READY) {
		_update_window_framebuf(source_service);
	}
	else if (command == AWM_UPDATE_WINDOW_TITLE) {
		awm_window_title_msg_t* title_msg =  (awm_window_title_msg_t*)user_message->body;
		_update_window_title(source_service, title_msg);
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

int main(int argc, char** argv) {
	amc_register_service("com.axle.awm");

	// Ask the kernel to map in the framebuffer and send us info about it
	amc__awm_map_framebuffer();

	amc_message_t* framebuf_info;
	amc_message_await("com.axle.core", &framebuf_info);
	framebuffer_info_t* framebuffer_info = (framebuffer_info_t*)framebuf_info->body;
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
	ca_layer* background = create_layer(screen_frame.size);
	_radial_gradiant(
		background, 
		background->size, 
		color_make(200, 150, 30), 
		color_make(150, 0, 0), 
		background->size.width/2.0, 
		background->size.height/2.0, 
		(float)background->size.height * 1.6
	);
    _screen.vmem = create_layer(screen_frame.size);

    printf("awm graphics: %d x %d, %d BPP @ 0x%08x\n", _screen.resolution.width, _screen.resolution.height, _screen.bits_per_pixel, _screen.physbase);

	ca_layer dummy_layer;
	dummy_layer.size = _screen.resolution;
	dummy_layer.raw = (uint8_t*)_screen.physbase;
	dummy_layer.alpha = 1.0;

	// Draw the background onto the screen buffer to start off
	blit_layer(_screen.vmem, background, screen_frame, screen_frame);
	blit_layer(&dummy_layer, _screen.vmem, screen_frame, screen_frame);

	while (true) {
		// Wait for a system event or window event
		amc_message_t* msg;
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
			if (!strcmp(source_service, "com.axle.kb_driver")) {
				handle_keystroke(msg);
				// Skip redrawing for now - the above will send a KB call to the
				// foremost program, which will later redraw its window with the new info
				continue;
			}
			else if (!strcmp(source_service, "com.axle.mouse_driver")) {
				// Update the mouse position based on the data packet
				handle_mouse_event(msg);
			}
			else {
				// TODO(PT): If a window sends REDRAW_READY, we can put it onto a "ready to redraw" list
				// Items can be popped off the list based on their Z-index, or a periodic time-based update
				// TODO(PT): If a window has requested multiple redraws within a single awm event loop
				// pass, awm should redraw it only once
				handle_user_message(msg);
			}
		} while (amc_has_message());

		// We're out of messages to process - composite everything together and redraw
		// First draw the background
		blit_layer(_screen.vmem, background, screen_frame, screen_frame);
		// Then each window (without copying in the window's current shared framebuffer)
		// Draw the bottom-most windows first
		// TODO(PT): Replace with a loop that draws the topmost window and 
		// splits lower windows into visible regions, then blits those
		for (int i = window_count-1; i >= 0; i--) {
			user_window_t* window = &windows[i];
			// As an optimization until we have visible-region splitting, skip drawing 
			// fully occluded windows
			bool fully_occluded = false;
			for (int j = i-1; j >= 0; j--) {
				user_window_t* higher_window = &windows[j];
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
				blit_layer(_screen.vmem, window->layer, window->frame, rect_make(point_zero(), window->frame.size));
			}
		}
		// And finally the cursor
		_draw_cursor();

		// Copy our internal screen buffer to video memory
		memcpy(_screen.physbase, _screen.vmem, _screen.resolution.width * _screen.resolution.height * _screen.bytes_per_pixel);
	}
	return 0;
}
