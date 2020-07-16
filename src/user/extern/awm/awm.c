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
	amc_register_service("com.axle.awm");

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
    //_screen.default_font_size = size_make(16, 16);

    //_screen.vmem = create_layer(_screen.resolution);
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

	//draw_rect(&dummy_layer, rect_make(point_make(200, 300), size_make(600, 400)), color_red(), THICKNESS_FILLED);
	draw_rect(&dummy_layer, rect_make(point_zero(), _screen.resolution), color_white(), THICKNESS_FILLED);
	draw_rect(&dummy_layer, rect_make(point_make(200, 300), size_make(600, 400)), color_red(), THICKNESS_FILLED);

	Point origin = point_make(40, 40);
	Point cursor = origin;
	Size font_size = size_make(40, 64);
	Size padding = size_make(1, 20);
	bool ready_to_redraw = false;
	while (true) {
		// Send redraw request to window
		if (ready_to_redraw) {
		}

		// Wait for a system event or window event
		amc_message_t msg = {0};
		const char* services[] = {"com.axle.kb_driver", "com.axle.tty", "com.user.window"};
		//const char* services[] = {"com.axle.kb_driver", "com.user.window"};
		amc_message_await_from_services(sizeof(services) / sizeof(services[0]), &services, &msg);

		// Process the command we just received
		// User requesting a window to draw in to?
		if (!strcmp(msg.source, "com.user.window")) {
			//printf("Received from %s: %s\n", msg.source, msg.data);
			if (!strcmp(msg.data, "get_framebuf")) {
				printf("Creating framebuffer for %s\n", msg.source);
				// const char* window_fbuf = "0xcafebabe";
				//amc_message_t* framebuf_msg = amc_message_construct(KEYSTROKE, window_fbuf, strlen(window_fbuf));
				uint32_t buffer_size = _screen.resolution.width * _screen.resolution.height * _screen.bytes_per_pixel;
				uint32_t local_buffer;
				uint32_t remote_buffer;
				amc_shared_memory_create("com.user.window", buffer_size, &local_buffer, &remote_buffer);
				printf("Created shared memory region. Local 0x%08x remote 0x%08x\n", local_buffer, remote_buffer);
				char buf[32];
				snprintf(&buf, 32, "0x%08x", remote_buffer);
				amc_message_t* framebuf_msg = amc_message_construct(KEYSTROKE, buf, sizeof(buf));
				amc_message_send("com.user.window", framebuf_msg);
				//asm("cli");asm("hlt");
				ready_to_redraw = true;
				char* redraw_cmdstr = "redraw";
				amc_message_t* redraw_cmd = amc_message_construct(KEYSTROKE, redraw_cmdstr, strlen(redraw_cmdstr));
				amc_message_send("com.user.window", redraw_cmd);
				continue;
			}
			else if (!strcmp(msg.data, "update_framebuf")) {
				//printf("Reading shared framebuffer\n");
				uint8_t* buffer_addr = (uint8_t*)0x0801a000;
				//asm("cli");asm("hlt");
				ca_layer remote_layer;
				remote_layer.size = _screen.resolution;
				remote_layer.raw = (uint8_t*)buffer_addr;
				remote_layer.alpha = 1.0;
				blit_layer(&dummy_layer, &remote_layer, rect_make(point_make(400, 400), size_make(500, 500)), rect_make(point_zero(), size_make(500, 500)));
				char* redraw_cmdstr = "redraw";
				amc_message_t* redraw_cmd = amc_message_construct(KEYSTROKE, redraw_cmdstr, strlen(redraw_cmdstr));
				amc_message_send("com.user.window", redraw_cmd);
				//amc_message_t* ack = amc_message_construct(KEYSTROKE, "ack", 4);
				//amc_message_send(msg.source, ack);
				//blit_layer(&dummy_layer, &remote_layer, rect_make(point_zero(), size_make(300, 300)), rect_make(point_zero(), size_make(300, 300)));

				continue;
			}
			else {
				printf("Unrecognized message: %s", msg.data);
				continue;
			}
		}
		//continue;

		Color draw_color;
		if (!strcmp(msg.source, "com.axle.kb_driver")) {
			draw_color = color_blue();
		}
		else if (!strcmp(msg.source, "com.axle.tty")) {
			draw_color = color_green();
		}
		else {
			printf("Unhandled message from window: 0x%08x\n", msg.data);
			continue;
		}

		for (int i = 0; i < msg.len; i++) {
			char ch = msg.data[i];
			draw_char(&dummy_layer, ch, cursor.x, cursor.y, draw_color, font_size);

			cursor.x += font_size.width + padding.width;
			if (cursor.x >= _screen.resolution.width || ch == '\n') {
				cursor.x = 0;
				cursor.y += font_size.height + padding.height;
			}

			if (cursor.y >= _screen.resolution.height) {
				draw_rect(&dummy_layer, rect_make(point_zero(), _screen.resolution), color_white(), THICKNESS_FILLED);
				cursor = origin;
			}
		}
	}
	//write_screen(gfx_screen());

	return 0;
}

