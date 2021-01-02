#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#include <kernel/amc.h>
#include <agx/lib/size.h>
#include <agx/lib/screen.h>
#include <agx/lib/shapes.h>
#include <agx/lib/ca_layer.h>
#include <agx/lib/putpixel.h>

#include "gfx.h"

// Many graphics lib functions call gfx_screen() 
Screen _screen = {0};
Screen* gfx_screen() {
	return &_screen;
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

// Replace it with another function if exists
float pifdist(int x1, int y1, int x2, int y2) {
	float x = x1 - x2;
	float y = y1 - y2;
	return sqrt(x * x + y * y);
}

void _radial_gradiant(ca_layer* layer, Size gradient_size, Color c1, Color c2, int x1, int y1, float r) {
	for (uint32_t y = 0; y < gradient_size.height; y++) {
		for (uint32_t x = 0; x < gradient_size.width; x++) {
			putpixel(layer, x, y, transcolor(c1, c2, pifdist(x1, y1, x, y) / r));
		}
	}
}

static void _handle_awm_message(amc_message_t awm_msg, ca_layer* window_layer) {
	Size window_size = size_make(800, 600);
	if (!strcmp(awm_msg.data, "redraw")) {
		/*
		int r = rand() % 255;
		int g = rand() % 255;
		int b = rand() % 255;
		Color c = color_make(r, g, b);
		draw_rect(window_layer, rect_make(point_zero(), size_make(800, 600)), c, THICKNESS_FILLED);
		*/
		Color c1 = color_make(rand() % 255, rand() % 255, rand() % 255);
		Color c2 = color_make(rand() % 255, rand() % 255, rand() % 255);
		//_radial_gradiant(window_layer, window_size, c1, c2, window_size.width/2, window_size.height / 2, (float)window_size.height/4);
		_radial_gradiant(window_layer, window_size, c1, c2, window_size.width/2, 200, (float)window_size.height/4);

		const char* cmd2 = "update_framebuf";
		amc_message_t* draw_framebuf_msg = amc_message_construct(cmd2, strlen(cmd2));
		amc_message_send("com.axle.awm", draw_framebuf_msg);
	}
}

int main(int argc, char** argv) {
	amc_register_service("com.user.rainbow");
	debug_awm();
	printf("Rainbow (PID [%d]) running!\n", getpid());
	
	const char* cmd = "get_framebuf";
	amc_message_t* get_framebuf_msg = amc_message_construct(cmd, strlen(cmd));
	amc_message_send("com.axle.awm", get_framebuf_msg);
	amc_message_t receive_framebuf = {0};
	amc_message_await("com.axle.awm", &receive_framebuf);
	uint32_t framebuffer_addr = (uint32_t)strtol(receive_framebuf.data, NULL, 0);
	//printf("Received framebuffer from awm: %s 0x%08x\n", receive_framebuf.data, framebuffer_addr);
	framebuffer_addr = 0xa0000000;
	uint8_t* buf = (uint8_t*)framebuffer_addr;

	// TODO(PT): Use an awm command to get screen info
	_screen.resolution = size_make(1920, 1080);
	_screen.physbase = (uint32_t*)framebuffer_addr;
	_screen.bits_per_pixel = 32;
	_screen.bytes_per_pixel = 4;

	ca_layer* dummy_layer = malloc(sizeof(ca_layer));
	memset(dummy_layer, 0, sizeof(dummy_layer));
	dummy_layer->size = _screen.resolution;
	dummy_layer->raw = (uint8_t*)framebuffer_addr;
	dummy_layer->alpha = 1.0;
    _screen.vmem = dummy_layer;

	draw_rect(_screen.vmem, rect_make(point_zero(), size_make(800, 600)), color_green(), THICKNESS_FILLED);

	int i = 0; 
	while (true) {
		i++;
		amc_message_t msg = {0};
		amc_message_await("com.axle.awm", &msg);

		if (!strcmp(msg.source, "com.axle.awm")) {
			_handle_awm_message(msg, dummy_layer);
		}
		else {
			printf("Unrecognized message: %s\n", msg.source);
			continue;
		}
	}

	return 0;
}

