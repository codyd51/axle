#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#include <agx/font/font.h>
#include <agx/lib/size.h>
#include <agx/lib/screen.h>
#include <agx/lib/shapes.h>
#include <agx/lib/ca_layer.h>
#include <agx/lib/putpixel.h>

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
	ca_layer* layer;
	//view_t* title_view;
	view_t* content_view;
	const char* owner_service;
} user_window_t;

user_window_t windows[32] = {0};
int window_count = 0;

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

static Point origin = {0};
static Point cursor = {0};
static Size font_size = {0};
static Size padding = {0};

static void _awm_overlay_putchar(char ch, Color color) {
	if (!origin.x) origin = point_make(40, 40);
	if (!cursor.x) cursor = point_make(40, 40);
	if (!font_size.width) font_size = size_make(40, 64);
	if (!padding.width) padding = size_make(1, 20);

	draw_char(_screen.vmem, ch, cursor.x, cursor.y, color, font_size);
	/*
	char x = (rand() % ('z' - 'a')) + 'a';
	printf("X: %c\n", x);
	draw_char(_screen.vmem, x, 0, 0, color, font_size);
	*/

	cursor.x += font_size.width + padding.width;
	if (cursor.x + font_size.width + padding.width >= _screen.resolution.width || ch == '\n') {
		cursor.x = 1;
		cursor.y += font_size.height + padding.height;
	}

	if (cursor.y + font_size.height + padding.height >= _screen.resolution.height) {
		//draw_rect(&dummy_layer, rect_make(point_zero(), _screen.resolution), color_white(), THICKNESS_FILLED);
		cursor = point_make(40, 40);
	}
}

static void handle_keystroke(amc_charlist_message_t* keystroke_msg) {
	Color keystroke_color = color_white();

	uint8_t len = amc_charlist_msg__get_len(keystroke_msg);
	char* data = amc_charlist_msg_data(keystroke_msg);
	for (int i = 0; i < len; i++) {
		char ch = data[i];
		_awm_overlay_putchar(ch, keystroke_color);
		printf("awm received keystroke: 0x%02x %c\n", ch, ch);
	}
}

static Point mouse_pos = {0};
static void handle_mouse_event(amc_charlist_message_t* mouse_event) {
	// TODO(PT): Add bytelist
	uint8_t* mouse_data = (uint8_t*)amc_charlist_msg_data(mouse_event);
	uint8_t state = mouse_data[0];
	int8_t rel_x = mouse_data[1];
	int8_t rel_y = mouse_data[2];
	printf("awm received mouse packet (state %d) (delta %d %d)\n", state, rel_x, rel_y);

	mouse_pos.x += rel_x;
	mouse_pos.y += rel_y;
	// Bind mouse to screen dimensions
	mouse_pos.x = max(0, mouse_pos.x);
	mouse_pos.y = max(0, mouse_pos.y);
	mouse_pos.x = min(_screen.resolution.width - 20, mouse_pos.x);
	mouse_pos.y = min(_screen.resolution.height - 20, mouse_pos.y);
}

static void _draw_cursor(void) {
	draw_rect(_screen.vmem, rect_make(mouse_pos, size_make(10, 10)), color_green(), THICKNESS_FILLED);
}

static void handle_stdout(amc_charlist_message_t* tty_msg) {
	Color stdout_color = color_purple();
	uint8_t len = amc_charlist_msg__get_len(tty_msg);
	char* charlist = (char*)amc_charlist_msg_data(tty_msg);
	for (uint8_t i = 0; i < len; i++) {
		char ch = charlist[i];
		_awm_overlay_putchar(ch, stdout_color);
	}
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

static void window_create(const char* owner_service) {
	int window_idx = window_count++;
	if (window_count > sizeof(windows) / sizeof(windows[0])) {
		assert("too many windows");
	}

	printf("Creating framebuffer for %s\n", owner_service);
	uint32_t buffer_size = _screen.resolution.width * _screen.resolution.height * _screen.bytes_per_pixel;
	uint32_t local_buffer;
	uint32_t remote_buffer;
	amc_shared_memory_create(owner_service, buffer_size, &local_buffer, &remote_buffer);

	printf("Created shared memory region for %s. Local 0x%08x - 0x%08x, remote 0x%08x - 0x%08x\n", owner_service, local_buffer, local_buffer + buffer_size, remote_buffer, remote_buffer + buffer_size);
	amc_command_ptr_msg__send(owner_service, AWM_CREATED_WINDOW_FRAMEBUFFER, remote_buffer);

	user_window_t* window = &windows[window_idx];
	window->frame = rect_make(point_make(200, 300), size_make(800, 600));
	window->layer = create_layer(_screen.resolution);
	printf("Raw window layer: 0x%08x 0x%08x\n", window->layer, window->layer->raw);

	Rect title_bar_frame = rect_make(point_zero(), size_make(window->frame.size.width, 24));
	int border_margin = 4;

	view_t* content_view = malloc(sizeof(view_t));
	content_view->frame = rect_make(point_make(border_margin, rect_max_y(title_bar_frame) + border_margin), size_make(window->frame.size.width - (border_margin*2), window->frame.size.height - rect_max_y(title_bar_frame) - (border_margin*2)));
	content_view->layer = malloc(sizeof(ca_layer));
	content_view->layer->size = _screen.resolution;
	content_view->layer->raw = (uint8_t*)local_buffer;
	content_view->layer->alpha = 1.0;
	window->content_view = content_view;
	printf("Content view window layer: 0x%08x\n", content_view->layer->raw);
	window->owner_service = owner_service;

	// Draw top window bar
	Color c1 = color_make(200, 160, 90);
	Color c2 = color_make(50, 50, 50);
	Color active = c1;
	Color inactive = c2;
	int block_size = 4;
	for (int y = 0; y < title_bar_frame.size.height; y++) {
		if (y % block_size == 0) {
			Color tmp = active;
			active = inactive;
			inactive = tmp;
		}
		for (int x = 0; x < title_bar_frame.size.width; x++) {
			if (x % block_size == 0) {
				Color tmp = active;
				active = inactive;
				inactive = tmp;
			}
			putpixel(window->layer, x, y, active);
		}
	}

	/*
	// Draw left border margin
	Line left_border = line_make(point_make(0, rect_max_y(title_bar_frame)), point_make(0, window->frame.size.height));
	draw_line(window->layer, left_border, color_green(), border_margin*2);
	Line right_border = line_make(point_make(rect_max_x(window->frame) - border_margin, rect_max_y(title_bar_frame)), point_make(rect_max_x(window->frame) - border_margin, window->frame.size.height));
	draw_line(window->layer, right_border, color_green(), border_margin*2);
	Line top_border = line_make(point_make(0, rect_max_y(title_bar_frame)), point_make(rect_max_x(window->frame), rect_max_y(title_bar_frame)));
	draw_line(window->layer, top_border, color_green(), border_margin*2);
	Line bottom_border = line_make(point_make(0, rect_max_y(window->frame) - border_margin), point_make(rect_max_x(window->frame), rect_max_y(window->frame) - border_margin));
	draw_line(window->layer, bottom_border, color_green(), border_margin*2);
	*/
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
	blit_layer(_screen.vmem, window->layer, window->frame, rect_make(point_zero(), window->frame.size));
	//blit_layer(_screen.physbase, window->content_view->layer, rect_make(point_zero(), _screen.resolution), rect_make(point_zero(), _screen.resolution));
}

static void handle_user_message(amc_command_message_t* user_message) {
	const char* source_service = amc_message_source(user_message);
	// User requesting a window to draw in to?
	if (amc_command_msg__get_command(user_message) == AWM_REQUEST_WINDOW_FRAMEBUFFER) {
		window_create(source_service);
		//_request_redraw(source_service);
		return;
	}
	else if (amc_command_msg__get_command(user_message) == AWM_WINDOW_REDRAW_READY) {
		_update_window_framebuf(source_service);
		//_request_redraw(source_service);
		return;
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
	draw_rect(background, screen_frame, color_orange(), THICKNESS_FILLED);
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
		amc_message_t msg_struct = {0};
		amc_message_t* msg = &msg_struct;
		do {
			// Wait until we've unblocked with at least one message available
			amc_message_await_any(msg);
			const char* source_service = amc_message_source(msg);

			// Process the message we just received
			if (!strcmp(source_service, "com.axle.kb_driver")) {
				handle_keystroke(msg);
				//memcpy(_screen.physbase, _screen.vmem, _screen.resolution.width * 50 * _screen.bytes_per_pixel);
				//continue;
			}
			else if (!strcmp(source_service, "com.axle.mouse_driver")) {
				// Update the mouse position based on the data packet
				handle_mouse_event(msg);
			}
			else if (!strcmp(source_service, "com.user.window") || !strcmp(source_service, "com.user.rainbow") || !strcmp(source_service, "com.axle.tty") || !strcmp(source_service, "com.user.paintbrush") || !strcmp(source_service, "com.user.textpad")) {
				// TODO(PT): If a window sends REDRAW_READY, we can put it onto a "ready to redraw" list
				// Items can be popped off the list based on their Z-index, or a periodic time-based update
				handle_user_message(msg);
			}
			else {
				printf("Unrecognized message from %s\n", source_service);
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
		// And finally the cursor
		_draw_cursor();

		// Copy our internal screen buffer to video memory
		memcpy(_screen.physbase, _screen.vmem, _screen.resolution.width * _screen.resolution.height * _screen.bytes_per_pixel);
	}
	return 0;
}
