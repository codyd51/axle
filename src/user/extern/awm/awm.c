#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
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

// Sorted by Z-index
user_window_t windows[32] = {0};
int window_count = 0;
static Point mouse_pos = {0};

static user_window_t* _window_move_to_top(user_window_t* window);

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

static void mouse_dispatch_events(uint8_t mouse_state, Point mouse_point, int8_t delta_x, int8_t delta_y) {
	static user_window_t* _prev_window_containing_mouse = NULL;
	static bool _in_left_click = false;
	static user_window_t* _left_click_window = NULL;

	// Is the left button clicked?
	if (mouse_state & (1 << 0)) {
		if (!_in_left_click) {
			printf("Begin left click\n");
			// TODO(PT): Delegate dispatch for "mouse movements" and "window dragging" systems
			_in_left_click = true;
			if (_prev_window_containing_mouse) {
				// Move this window to the top of the stack
				_prev_window_containing_mouse = _window_move_to_top(_prev_window_containing_mouse);
				_left_click_window = _prev_window_containing_mouse;
			}
		}
		else {
			//printf("Got packet within left click\n");
		}
	}
	else {
		if (_in_left_click) {
			printf("End left click\n");
			_in_left_click = false;
			_left_click_window = NULL;
		}
	}

	// Check if we're still overlapping with the previous window
	if (_prev_window_containing_mouse) {
		if (!rect_contains_point(_prev_window_containing_mouse->frame, mouse_point)) {
			printf("Mouse exited %s\n", _prev_window_containing_mouse->owner_service);
			amc_msg_u32_1__send(_prev_window_containing_mouse->owner_service, AWM_MOUSE_EXITED);
			// Reset the cursor state
			_prev_window_containing_mouse = NULL;
			// TODO(PT): Everything to do with "left click state" should be managed within delegate functions
			// TODO(PT): As well as everything to do with "window events"
			_left_click_window = NULL;
		}
		else {
			// We've moved the mouse within a window
			// Try to see if we've just entered a higher window
			uint32_t current_window_pos = _prev_window_containing_mouse - windows;
			for (int i = 0; i < current_window_pos; i++) {
				user_window_t* higher_window = &windows[i];
				if (rect_contains_point(higher_window->frame, mouse_point)) {
					printf("Mouse exited %s\n", _prev_window_containing_mouse->owner_service);
					amc_msg_u32_1__send(_prev_window_containing_mouse->owner_service, AWM_MOUSE_EXITED);
					// Reset the cursor state
					_prev_window_containing_mouse = NULL;
					// TODO(PT): Everything to do with "left click state" should be managed within delegate functions
					// TODO(PT): As well as everything to do with "window events"
					_left_click_window = NULL;

					printf("Entered higher window %s\n", higher_window->owner_service);
					// Inform the window the mouse has just entered it
					amc_msg_u32_1__send(higher_window->owner_service, AWM_MOUSE_ENTERED);
					// Keep track that we're currently within this window
					_prev_window_containing_mouse = higher_window;
					break;
				}
			}

			Point local_mouse = point_make(mouse_point.x - _prev_window_containing_mouse->frame.origin.x, mouse_point.y - _prev_window_containing_mouse->frame.origin.y);
			amc_msg_u32_3__send(_prev_window_containing_mouse->owner_service, AWM_MOUSE_MOVED, local_mouse.x, local_mouse.y);

			if (_left_click_window) {
				//printf("Moving dragged window\n");
				_prev_window_containing_mouse->frame.origin.x += delta_x;
				_prev_window_containing_mouse->frame.origin.y += delta_y;
			}
		}
		return;
	}

	// Check each window and see if we've just entered it
	// This array is sorted in Z-order so we encounter the topmost window first
	for (int i = 0; i < window_count; i++) {
		user_window_t* window = &windows[i];
		if (rect_contains_point(window->frame, mouse_point)) {
			// Inform the window the mouse has just entered it
			amc_msg_u32_1__send(window->owner_service, AWM_MOUSE_ENTERED);
			// Keep track that we're currently within this window
			_prev_window_containing_mouse = window;
			printf("Mouse entered %s\n", window->owner_service);
			return;
		}
	}
}

static void handle_mouse_event(amc_message_t* mouse_event) {
	int8_t state = mouse_event->body[0];
	int8_t rel_x = mouse_event->body[1];
	int8_t rel_y = mouse_event->body[2];
	//printf("awm received mouse packet (state %d) (delta %d %d)\n", state, rel_x, rel_y);

	mouse_pos.x += rel_x;
	mouse_pos.y += rel_y;
	// Bind mouse to screen dimensions
	mouse_pos.x = max(0, mouse_pos.x);
	mouse_pos.y = max(0, mouse_pos.y);
	mouse_pos.x = min(_screen.resolution.width - 20, mouse_pos.x);
	mouse_pos.y = min(_screen.resolution.height - 20, mouse_pos.y);

	mouse_dispatch_events(state, mouse_pos, rel_x, rel_y);
}

static void _draw_cursor(void) {
	// Re-draw the background where the mouse has just left
	Size cursor_size = size_make(14, 14);
	//Rect old_mouse_rect = rect_make(old_mouse_pos, cursor_size);
	//blit_layer(_screen.vmem, background, old_mouse_rect, old_mouse_rect);

	// Draw the new cursor
	Rect new_mouse_rect = rect_make(mouse_pos, cursor_size);
	draw_rect(_screen.vmem, new_mouse_rect, color_black(), THICKNESS_FILLED);
	draw_rect(_screen.vmem, rect_make(point_make(new_mouse_rect.origin.x + 2, new_mouse_rect.origin.y + 2), size_make(10, 10)), color_green(), THICKNESS_FILLED);

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

static void window_create(const char* owner_service, uint32_t width, uint32_t height) {
	int window_idx = window_count++;
	if (window_count > sizeof(windows) / sizeof(windows[0])) {
		assert("too many windows");
	}

	printf("Creating framebuffer for %s\n", owner_service);
	// TODO(PT): By always making the buffer the size of the screen, we allow for window resizing later
	uint32_t buffer_size = _screen.resolution.width * _screen.resolution.height * _screen.bytes_per_pixel;
	uint32_t local_buffer;
	uint32_t remote_buffer;
	amc_shared_memory_create(owner_service, buffer_size, &local_buffer, &remote_buffer);

	printf("AWM made shared framebuffer for %s\n", owner_service);
	printf("\tAWM    memory: 0x%08x - 0x%08x\n", local_buffer, local_buffer + buffer_size);
	printf("\tRemote memory: 0x%08x - 0x%08x\n", remote_buffer, remote_buffer + buffer_size);
	amc_msg_u32_2__send(owner_service, AWM_CREATED_WINDOW_FRAMEBUFFER, remote_buffer);

	user_window_t* window = &windows[window_idx];
	// Place the window in the center of the screen
	Point origin = point_make(
		(_screen.resolution.width / 2) - (width / 2),
		(_screen.resolution.height / 2) - (height / 2)
	);
	// Make the window a bit bigger than the user requested to accomodate for decorations
	int border_margin = 4;
	int full_window_width = width + (border_margin * 2);
	Size title_bar_size = size_make(full_window_width, 24);
	Size full_window_size = size_make(
		full_window_width, 
		height + title_bar_size.height + (border_margin * 2)
	);

	window->frame = rect_make(origin, full_window_size);
	window->layer = create_layer(_screen.resolution);
	printf("Raw window layer: 0x%08x 0x%08x\n", window->layer, window->layer->raw);

	view_t* content_view = malloc(sizeof(view_t));
	content_view->frame = rect_make(
		point_make(
			border_margin, 
			title_bar_size.height + border_margin
		), 
		size_make(width, height)
	);

	content_view->layer = malloc(sizeof(ca_layer));
	content_view->layer->size = _screen.resolution;
	content_view->layer->raw = (uint8_t*)local_buffer;
	content_view->layer->alpha = 1.0;
	window->content_view = content_view;
	printf("Content view window layer: 0x%08x\n", content_view->layer->raw);
	// Copy the owner service name as we don't own it
	window->owner_service = strndup(owner_service, AMC_MAX_SERVICE_NAME_LEN);
	printf("set window owner_service %s\n", owner_service);

	// Configure the title text box
	window->title_text_box = text_box_create(title_bar_size, color_black());

	// Draw top window bar
	Color c1 = color_make(200, 160, 90);
	Color c2 = color_make(50, 50, 50);
	Color active = c1;
	Color inactive = c2;
	int block_size = 4;
	for (int y = 0; y < title_bar_size.height; y++) {
		if (y % block_size == 0) {
			Color tmp = active;
			active = inactive;
			inactive = tmp;
		}
		for (int x = 0; x < title_bar_size.width; x++) {
			if (x % block_size == 0) {
				Color tmp = active;
				active = inactive;
				inactive = tmp;
			}
			putpixel(window->layer, x, y, active);
		}
	}

	// Draw window title
	uint32_t title_len = ((window->title_text_box->font_size.width + window->title_text_box->font_padding.width) * strlen(owner_service));
	Point title_text_origin = point_make(border_margin, border_margin);
	for (int i = 0; i < strlen(owner_service); i++) {
		text_box_putchar(window->title_text_box, owner_service[i], color_white());
	}
	Size visible_title_bar_size = size_make(title_len, 14);
	blit_layer(
		window->layer, 
		window->title_text_box->layer, 
		rect_make(
			point_make(
				title_text_origin.x - 2,
				title_text_origin.y - 2
			),
			visible_title_bar_size
		),
		rect_make(point_zero(), visible_title_bar_size)
	);

	// Make the new window show up on top
	_window_move_to_top(window);
}

static void _request_redraw(const char* owner_service) {
	char* redraw_cmdstr = "redraw";
	amc_message_t* redraw_cmd = amc_message_construct(redraw_cmdstr, strlen(redraw_cmdstr));
	//amc_message_send(owner_service, redraw_cmd);
}

static void _update_window_framebuf_idx(int idx) {
	if (idx >= window_count) assert("invalid index");
	user_window_t* window = &windows[idx];
	//blit_layer(_screen.vmem, &window->shared_layer, window->frame, rect_make(point_zero(), window->frame.size));
	//blit_layer(_screen.vmem, window->layer, window->frame, rect_make(point_zero(), window->frame.size));
	blit_layer(window->layer, window->content_view->layer, window->content_view->frame, rect_make(point_zero(), window->content_view->frame.size));
	blit_layer(_screen.vmem, window->layer, window->frame, rect_make(point_zero(), window->frame.size));
}

static void _update_window_framebuf(const char* owner_service) {
	user_window_t* window = _window_for_service(owner_service);
	if (!window) {
		printf("Failed to find a window for %s\n", owner_service);
		return;
	}

	//printf("Blitting layer for %s: 0x%08x -> 0x%08x\n", owner_service, window->layer->raw, _screen.vmem->raw);
	//blit_layer(_screen.vmem, window->layer, window->frame, rect_make(point_zero(), window->frame.size));

	Rect title_bar_frame = rect_make(point_zero(), size_make(window->frame.size.width, 40));
	//blit_layer(window->layer, window->content_view->layer, window->content_view->frame, rect_make(point_zero(), window->content_view->frame.size));
	//blit_layer(window->layer, window->content_view->layer, rect_make(point_make(100, 60), size_make(200, 400)), rect_make(point_zero(), size_make(200, 400)));

	//blit_layer(window->layer, window->content_view->layer, rect_make(point_zero(), size_make(200, 400)), rect_make(point_zero(), size_make(200, 400)));
	blit_layer(window->layer, window->content_view->layer, window->content_view->frame, rect_make(point_zero(), window->content_view->frame.size));
	//blit_layer(_screen.vmem, window->layer, window->frame, rect_make(point_zero(), window->frame.size));
	//blit_layer(_screen.physbase, window->content_view->layer, rect_make(point_zero(), _screen.resolution), rect_make(point_zero(), _screen.resolution));
}

static void handle_user_message(amc_command_message_t* user_message) {
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
		return;
	}
	else {
		printf("Unknown message from %s: %d\n", source_service, command);
	}
}

int main(int argc, char** argv) {
	amc_register_service("com.axle.awm");

	char* framebuffer_addr_str = argv[1];
	uint32_t framebuffer_addr = (uint32_t)strtol(framebuffer_addr_str, NULL, 0);
	printf("Got framebuffer addr %s 0x%08x\n", framebuffer_addr_str, framebuffer_addr);

	framebuffer_info_t* framebuffer_info = (framebuffer_info_t*)framebuffer_addr;
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
	draw_rect(background, screen_frame, color_white(), THICKNESS_FILLED);
    _screen.vmem = create_layer(screen_frame.size);

	/*
    _screen.window = create_window_int(rect_make(point_make(0, 0), _screen.resolution), true);
    _screen.window->superview = NULL;
    _screen.surfaces = array_m_create(128);
	*/

    printf("Graphics: %d x %d, %d BPP @ 0x%08x\n", _screen.resolution.width, _screen.resolution.height, _screen.bits_per_pixel, _screen.physbase);

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
			const char* source_service = amc_message_source(msg);

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
			blit_layer(_screen.vmem, window->layer, window->frame, rect_make(point_zero(), window->frame.size));
		}
		// Draw a VStack of currently active applications, so the user is aware
		// TODO(PT): Move this to a `window_order_changed` delegate
		Size vstack_row_size = size_make(260, 30);
		uint32_t vstack_y = _screen.resolution.height - vstack_row_size.height;
		for (int i = window_count-1; i >= 0; i--) {
			user_window_t* window = &windows[i];
			Rect box = rect_make(
				point_make(0, vstack_y),
				vstack_row_size
			);

			text_box_t* text_box = text_box_create(vstack_row_size, color_black());
			text_box->font_size = size_make(10, 10);
			text_box->font_padding = size_make(0, 2);
			text_box_puts(text_box, window->owner_service, color_white());
			blit_layer(
				_screen.vmem, 
				text_box->layer, 
				rect_make(
					point_make(box.origin.x, vstack_y + 2), 
					vstack_row_size
				),
				rect_make(
					point_zero(), 
					vstack_row_size
				)
			);
			text_box_destroy(text_box);

			vstack_y -= vstack_row_size.height;
		}

		// And finally the cursor
		_draw_cursor();

		// Copy our internal screen buffer to video memory
		memcpy(_screen.physbase, _screen.vmem, _screen.resolution.width * _screen.resolution.height * _screen.bytes_per_pixel);
	}
	return 0;
}
