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
#include <agx/lib/text_box.h>

// Window management
#include <awm/awm.h>

// Communication with other processes
#include <libamc/libamc.h>

#include "gfx.h"

// Many graphics lib functions call gfx_screen() 
Screen _screen = {0};
Screen* gfx_screen() {
	return &_screen;
}

static ca_layer* window_layer_get(uint32_t width, uint32_t height) {
	// Ask awm to make a window for us
	amc_msg_u32_3__send("com.axle.awm", AWM_REQUEST_WINDOW_FRAMEBUFFER, width, height);

	// And get back info about the window it made
	amc_message_t* receive_framebuf;
	amc_message_await("com.axle.awm", &receive_framebuf);
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

	Size window_size = size_make(900, 1000);
	Rect window_frame = rect_make(point_zero(), window_size);
	ca_layer* window_layer = window_layer_get(window_size.width, window_size.height);
	draw_rect(window_layer, window_frame, color_light_gray(), THICKNESS_FILLED);

	int text_box_padding = 6;
	Rect text_box_frame = rect_make(
		point_make(
			text_box_padding,
			text_box_padding
		),
		size_make(
			window_size.width - (text_box_padding * 2), 
			window_size.height - (text_box_padding * 2)
		)
	);

	text_box_t* text_box = text_box_create(text_box_frame.size, color_white());
	text_box->preserves_history = true;

	while (true) {
		amc_message_t* msg;
		bool got_resize_msg = false;
		awm_window_resized_msg_t newest_resize_msg = {0};
		do {
			// Wait until we've unblocked with at least one message available
			amc_message_await_any(&msg);
			if (libamc_handle_message(msg)) {
				continue;
			}

			const char* source_service = amc_message_source(msg);
			if (!strcmp(source_service, "com.axle.core")) {
				char buf[msg->len+1];
				strncpy(buf, msg->body, msg->len);
				buf[msg->len] = '\0';
				text_box_puts(text_box, buf, color_make(135, 20, 20));
			}
			else if (!strcmp(source_service, "com.axle.awm")) {
				uint32_t cmd = amc_msg_u32_get_word(msg, 0);
				if (cmd == AWM_MOUSE_SCROLLED) {
					awm_mouse_scrolled_msg_t* m = (awm_mouse_scrolled_msg_t*)msg->body;
					bool scroll_up = m->delta_z > 0;
					for (uint32_t i = 0; i < abs(m->delta_z); i++) {
						if (scroll_up) text_box_scroll_up(text_box);
						else text_box_scroll_down(text_box);
					}
				}
				else if (cmd == AWM_WINDOW_RESIZED) {
					got_resize_msg = true;
					awm_window_resized_msg_t* m = (awm_window_resized_msg_t*)msg->body;
					newest_resize_msg = *m;
				}
			}
		} while (amc_has_message());

		if (got_resize_msg) {
			window_size = newest_resize_msg.new_size;
			window_frame.size = window_size;
			text_box_frame = rect_make(
				point_make(
					text_box_padding,
					text_box_padding
				),
				size_make(
					window_size.width - (text_box_padding * 2), 
					window_size.height - (text_box_padding * 2)
				)
			);
			draw_rect(window_layer, window_frame, color_light_gray(), text_box_padding);
			text_box_resize(text_box, text_box_frame.size);
		}

		// Blit the text box to the window layer
		text_box_blit(text_box, window_layer, text_box_frame);
		// We're out of messages to process - ask awm to redraw the window with our updates
		amc_msg_u32_1__send("com.axle.awm", AWM_WINDOW_REDRAW_READY);
	}
	
	return 0;
}
