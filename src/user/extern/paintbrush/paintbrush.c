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
	amc_register_service("com.user.paintbrush");

	printf("Paintbrush (PID [%d]) running!\n", getpid());
	Size window_size = size_make(1000, 800);
	ca_layer* window_layer = window_layer_get(window_size.width, window_size.height);
	draw_rect(window_layer, rect_make(point_zero(), window_size), color_brown(), THICKNESS_FILLED);
	amc_command_msg__send("com.axle.awm", AWM_WINDOW_REDRAW_READY);

	while (true) {
		// TODO(PT): An "await for event matching" so we can wait for mouse entered / mouse exited
		asm("sti");

		amc_command_message_t msg = {0};

		bool did_draw = false;
		int i = 0;
		do {
			// Wait until we've unblocked with at least one message available
			amc_message_await("com.axle.awm", &msg);
			i++;
			if (i>10) break;

			// TODO(PT): Need a struct type selector
			uint32_t event = amc_command_ptr_msg__get_command(&msg);
			if (event == AWM_MOUSE_ENTERED) {
				//draw_rect(window_layer, rect_make(point_zero(), window_size), color_green(), THICKNESS_FILLED);
			}
			else if (event == AWM_MOUSE_MOVED) {
				uint32_t* data = (uint32_t*)amc_command_msg_data(&msg);
				uint32_t x = data[0];
				uint32_t y = data[1];
				draw_circle(window_layer, circle_make(point_make(x, y - 20), 20), color_black(), THICKNESS_FILLED);
				did_draw = true;
			}
			else if (event == AWM_MOUSE_EXITED) {
				draw_rect(window_layer, rect_make(point_zero(), window_size), color_brown(), THICKNESS_FILLED);
				did_draw = true;
			}
		} while (amc_has_message_from("com.axle.awm"));

		if (did_draw) {
			amc_command_msg__send("com.axle.awm", AWM_WINDOW_REDRAW_READY);
		}
	}

	return 0;
}

