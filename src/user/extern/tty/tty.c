#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

// Layers and drawing
#include <agx/lib/size.h>
#include <agx/lib/screen.h>
#include <agx/lib/shapes.h>
#include <agx/lib/ca_layer.h>
#include <agx/lib/putpixel.h>

// Window management
#include <awm/awm.h>

// Communication with other processes
#include <libamc/libamc.h>

#include "gfx.h"

typedef struct text_box {
	ca_layer* layer;
	Size size;
	Point origin;
	Size font_size;
	Size font_padding;
	Point cursor_pos;
} text_box_t;

// Many graphics lib functions call gfx_screen() 
Screen _screen = {0};
Screen* gfx_screen() {
	return &_screen;
}

static void _handle_awm_message(amc_charlist_message_t awm_msg) {
	if (!strcmp(awm_msg.body.charlist.data, "redraw")) {
		const char* cmd2 = "update_framebuf";
		amc_message_t* draw_framebuf_msg = amc_message_construct(cmd2, strlen(cmd2));
		amc_message_send("com.axle.awm", draw_framebuf_msg);
	}
}

static void _newline(text_box_t* text_box) {
	text_box->cursor_pos.x = text_box->origin.x;
	text_box->cursor_pos.y += text_box->font_size.height + text_box->font_padding.height;

	if (text_box->cursor_pos.y >= text_box->size.height) {
		draw_rect(text_box->layer, rect_make(text_box->origin, text_box->size), color_white(), THICKNESS_FILLED);
		text_box->cursor_pos = text_box->origin;
	}
}

static void _putchar(text_box_t* text_box, char ch, Color color) {
	if (ch == '\n') {
		_newline(text_box);
		return;
	}
	draw_char(text_box->layer, ch, text_box->cursor_pos.x, text_box->cursor_pos.y, color, text_box->font_size);

	text_box->cursor_pos.x += text_box->font_size.width + text_box->font_padding.width;
	if (text_box->cursor_pos.x >= text_box->size.width) {
		_newline(text_box);
		return;
	}
}

static void _handle_tty_message(text_box_t* text_box, amc_charlist_message_t tty_msg) {
	Color stdout_color = color_purple();
	text_box->cursor_pos.x = 0;
	for (int i = 0; i < tty_msg.body.charlist.len; i++) {
		char ch = tty_msg.body.charlist.data[i];
		_putchar(text_box, ch, color_black());
	}
}

static ca_layer* window_layer_get(uint32_t width, uint32_t height) {
	// Ask awm to make a window for us
	amc_msg_u32_3__send("com.axle.awm", AWM_REQUEST_WINDOW_FRAMEBUFFER, width, height);
	// And get back info about the window it made
	amc_command_ptr_message_t receive_framebuf = {0};
	amc_message_await("com.axle.awm", &receive_framebuf);
	// TODO(PT): Need a struct type selector
	if (amc_command_ptr_msg__get_command(&receive_framebuf) != AWM_CREATED_WINDOW_FRAMEBUFFER) {
		printf("Invalid state. Expected framebuffer command\n");
	}

	printf("Received framebuffer from awm: %d 0x%08x\n", amc_command_ptr_msg__get_command(&receive_framebuf), amc_command_ptr_msg__get_ptr(&receive_framebuf));
	uint32_t framebuffer_addr = receive_framebuf.body.cmd_ptr.ptr_val;
	uint8_t* buf = (uint8_t*)framebuffer_addr;

	// TODO(PT): Use an awm command to get screen info
	_screen.resolution = size_make(1920, 1080);
	_screen.physbase = (uint32_t*)0;
	_screen.bits_per_pixel = 32;
	_screen.bytes_per_pixel = 4;

	ca_layer* dummy_layer = malloc(sizeof(ca_layer));
	memset(dummy_layer, 0, sizeof(dummy_layer));
	dummy_layer->size = _screen.resolution;
	dummy_layer->raw = (uint8_t*)framebuffer_addr;
	dummy_layer->alpha = 1.0;
    _screen.vmem = dummy_layer;

	return dummy_layer;
}

int main(int argc, char** argv) {
	amc_register_service("com.axle.tty");

	Size window_size = size_make(900, 900);
	ca_layer* window_layer = window_layer_get(window_size.width, window_size.height);

	text_box_t* text_box = malloc(sizeof(text_box_t));
	memset(text_box, 0, sizeof(text_box_t));
	text_box->layer = window_layer;
	text_box->origin = point_make(6, 6);
	text_box->size = size_make(window_size.width - (text_box->origin.x * 2), window_size.height - (text_box->origin.y * 2));
	text_box->cursor_pos = text_box->origin;
	text_box->font_size = size_make(10, 10);
	text_box->font_padding = size_make(0, 2);

	draw_rect(window_layer, rect_make(point_zero(), window_size), color_white(), THICKNESS_FILLED);

	while (true) {
		amc_charlist_message_t msg = {0};
		// TODO(PT): Eat messages that aren't from core (like awm status messages)
		do {
			// Wait until we've unblocked with at least one message available
			amc_message_await("com.axle.core", &msg);

			// We've got at least one message available
			// Process each message in our inbox
			for (int i = 0; i < msg.body.charlist.len; i++) {
				char ch = msg.body.charlist.data[i];
				_putchar(text_box, ch, color_make(135, 20, 20));
			}
		} while (amc_has_message_from("com.axle.core"));
		// We're out of messages to process - ask awm to redraw the window with our updates
		amc_command_msg__send("com.axle.awm", AWM_WINDOW_REDRAW_READY);
	}
	
	return 0;
}
