#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "amc.h"
#include "gfx.h"
#include "lib/size.h"
#include "lib/screen.h"
#include "lib/shapes.h"
#include "lib/ca_layer.h"

Screen _screen = {0};

Screen* gfx_screen() {
	if (_screen.physbase > 0) return &_screen;
	return NULL;
}

void write_screen(Screen* screen) {
    //vsync();
    uint8_t* raw_double_buf = screen->vmem->raw;
    memcpy(screen->physbase, screen->vmem->raw, screen->resolution.width * screen->resolution.height * 3);
}

int main(int argc, char** argv) {
	char* framebuffer_addr_str = argv[1];
	uint32_t framebuffer_addr = (uint32_t)strtol(framebuffer_addr_str, NULL, 0);
	printf("Got framebuffer addr %s 0x%08x\n", framebuffer_addr_str, framebuffer_addr);

	framebuffer_info_t* framebuffer_info = (framebuffer_info_t*)framebuffer_addr;

    _screen.physbase = (uint32_t*)framebuffer_info->address;
    _screen.video_memory_size = framebuffer_info->size;

    _screen.resolution = size_make(framebuffer_info->width, framebuffer_info->height);
    _screen.bits_per_pixel = framebuffer_info->bits_per_pixel;
    _screen.bytes_per_pixel = framebuffer_info->bytes_per_pixel;

    // Font size is calculated as a fraction of screen size
    //_screen.default_font_size = font_size_for_resolution(_screen.resolution);

    //_screen.vmem = create_layer(_screen.resolution);
	/*
    _screen.window = create_window_int(rect_make(point_make(0, 0), _screen.resolution), true);
    _screen.window->superview = NULL;
    _screen.surfaces = array_m_create(128);
	*/

    printf("Graphics: %d x %d, %d BPP\n", _screen.resolution.width, _screen.resolution.height, _screen.bits_per_pixel);
	ca_layer dummy_layer;
	dummy_layer.size = _screen.resolution;
	dummy_layer.raw = (uint8_t*)_screen.physbase;
	dummy_layer.alpha = 1.0;

	//draw_rect(_screen.vmem, rect_make(point_make(200, 300), size_make(600, 400)), color_red(), THICKNESS_FILLED);
	draw_rect(&dummy_layer, rect_make(point_zero(), _screen.resolution), color_white(), THICKNESS_FILLED);
	draw_rect(&dummy_layer, rect_make(point_make(200, 300), size_make(600, 400)), color_red(), THICKNESS_FILLED);

	amc_register_service("com.axle.awm");
	Point origin = point_make(40, 40);
	Point cursor = origin;
	Size font_size = size_make(40, 64);
	Size padding = size_make(1, 20);
	while (true) {
		amc_message_t msg = {0};
		amc_message_await("com.axle.kb_driver", &msg);
		draw_char(&dummy_layer, msg.data[0], cursor.x, cursor.y, color_blue(), font_size);

		cursor.x += font_size.width + padding.width;
		if (cursor.x >= _screen.resolution.width) {
			cursor.x = 0;
			cursor.y += font_size.height + padding.height;
		}

		if (cursor.y >= _screen.resolution.height) {
			draw_rect(&dummy_layer, rect_make(point_zero(), _screen.resolution), color_white(), THICKNESS_FILLED);
			cursor = origin;
		}
	}
	//write_screen(gfx_screen());

	return 0;
}

