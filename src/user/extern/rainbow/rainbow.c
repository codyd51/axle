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

#include <awm/awm.h>

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
		//printf("draw row %d\n", y);
		for (uint32_t x = 0; x < gradient_size.width; x++) {
			putpixel(layer, x, y, transcolor(c1, c2, pifdist(x1, y1, x, y) / r));
		}
	}
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
	amc_register_service("com.user.rainbow");

	printf("Rainbow (PID [%d]) running!\n", getpid());
	Size window_size = size_make(300, 300);
	ca_layer* window_layer = window_layer_get(window_size.width, window_size.height);
	draw_rect(window_layer, rect_make(point_zero(), window_size), color_red(), THICKNESS_FILLED);
	amc_msg_u32_1__send("com.axle.awm", AWM_WINDOW_REDRAW_READY);

	while (true) {
		// TODO(PT): For some reason interrupts are disabled here - fix it
		asm("sti");
		Color c1 = color_make(rand() % 255, rand() % 255, rand() % 255);
		Color c2 = color_make(rand() % 255, rand() % 255, rand() % 255);
		//_radial_gradiant(window_layer, window_size, c1, c2, window_size.width/2, window_size.height / 2, (float)window_size.height/4);
		_radial_gradiant(window_layer, window_size, c1, c2, window_size.width/2, window_size.height/2, (float)window_size.height/4);
		amc_msg_u32_1__send("com.axle.awm", AWM_WINDOW_REDRAW_READY);
	}

	return 0;
}

