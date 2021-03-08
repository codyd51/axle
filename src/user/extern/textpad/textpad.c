#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#include <libamc/libamc.h>

#include <agx/lib/size.h>
#include <agx/lib/screen.h>
#include <agx/lib/shapes.h>
#include <agx/lib/ca_layer.h>
#include <agx/lib/putpixel.h>
#include <agx/lib/text_box.h>

#include <awm/awm.h>

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
	amc_register_service("com.user.textpad");

	printf("Textpad (PID [%d]) running!\n", getpid());
	Size window_size = size_make(400, 200);
	ca_layer* window_layer = window_layer_get(window_size.width, window_size.height);

	text_box_t* text_box = text_box_create(window_size, color_dark_gray());
	text_box->font_size = size_make(24, 24);
	text_box->font_padding = size_make(0, 2);
	// Blit the text box the first time to get everything set up before our first redraw
	Rect window_frame = rect_make(point_zero(), window_size);
	blit_layer(window_layer, text_box->layer, window_frame, window_frame);

	amc_msg_u32_1__send("com.axle.awm", AWM_WINDOW_REDRAW_READY);

	int i = 0;
	while (true) {
		// TODO(PT): An "await for event matching" so we can wait for mouse entered / mouse exited
		amc_message_t* msg;
		do {
			amc_message_await("com.axle.awm", &msg);
			// TODO(PT): Need a struct type selector
			uint32_t event = amc_msg_u32_get_word(msg, 0);
			if (event == AWM_KEY_DOWN) {
				char ch = (char)amc_msg_u32_get_word(msg, 1);
			}
		} while (amc_has_message_from("com.axle.awm"));
		// Blit the text box to the window layer
		blit_layer(window_layer, text_box->layer, window_frame, window_frame);
		// All messages have been processed - ask awm to redraw the window
		amc_msg_u32_1__send("com.axle.awm", AWM_WINDOW_REDRAW_READY);
	}

	return 0;
}
