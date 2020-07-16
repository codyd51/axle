#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

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

int main(int argc, char** argv) {
	amc_register_service("com.user.window");
	printf("User window (PID [%d]) running!\n", getpid());
	
	const char* cmd = "get_framebuf";
	amc_message_t* get_framebuf_msg = amc_message_construct(cmd, strlen(cmd));
	amc_message_send("com.axle.awm", get_framebuf_msg);
	amc_message_t receive_framebuf = {0};
	amc_message_await("com.axle.awm", &receive_framebuf);
	uint32_t framebuffer_addr = (uint32_t)strtol(receive_framebuf.data, NULL, 0);
	printf("Received framebuffer from awm: 0x%08x\n", framebuffer_addr);

	// TODO(PT): Use an awm command to get screen info
	_screen.resolution = size_make(1920, 1080);
	_screen.bits_per_pixel = 32;
	_screen.bytes_per_pixel = 4;

	ca_layer dummy_layer;
	dummy_layer.size = size_make(1920, 1080);
	dummy_layer.raw = (uint8_t*)framebuffer_addr;
	dummy_layer.alpha = 1.0;
	//memset((uint8_t*)framebuffer_addr, 0, 300*300*4);
	//memset((uint32_t*)0x0a000000, 0xffbb9922, 900*800);

	int i = 0; 
	while (true) {
		i++;
		amc_message_t cmd;
		amc_message_await("com.axle.awm", &cmd);
		//printf("cmd.data %s\n", cmd.data);
		if (!strcmp(cmd.data, "redraw")) {
			draw_rect(&dummy_layer, rect_make(point_make(100, 100), size_make(100, 100)), color_purple(), THICKNESS_FILLED);
			int x = 10 + (rand() % 450);
			int y = 10 + (rand() % 450);
			int w = 10 + (rand() % 450);
			int h = 10 + (rand() % 450);
			int r = (rand() % 255);
			int g = (rand() % 255);
			int b = (rand() % 255);
			draw_rect(&dummy_layer, rect_make(point_make(x, y), size_make(w, h)), color_make(r, g, b), THICKNESS_FILLED);
			if (true || i%10 == 0) {
				const char* cmd2 = "update_framebuf";
				amc_message_t* draw_framebuf_msg = amc_message_construct(cmd2, strlen(cmd2));
				amc_message_send("com.axle.awm", draw_framebuf_msg);
			}
			continue;
		}
		else {
			printf("Unrecognized command: %s\n", cmd.data);
			continue;
		}
	}
	//§asm("cli");asm("hlt");

	//draw_rect(_screen.vmem, rect_make(point_make(200, 300), size_make(600, 400)), color_red(), THICKNESS_FILLED);
	//uint8_t* framebuffer_buf = 0x0a000000;
	//dummy_layer.raw = framebuffer_buf;
	/*
	for (int x = 0; x < 100; x++) {
		for (int y = 0; y < 100; y++) {
			putpixel(&dummy_layer, x, y, color_purple());
		}
	}
	*/
	//draw_rect(&dummy_layer, rect_make(point_zero(), dummy_layer.size), color_blue(), THICKNESS_FILLED);

	/*
	const char* cmd2 = "update_framebuf";
	amc_message_t* draw_framebuf_msg = amc_message_construct(cmd2, strlen(cmd2));
	amc_message_send("com.axle.awm", draw_framebuf_msg);
	*/
	//draw_rect(&dummy_layer, rect_make(point_zero(), size_make(500, 500)), color_white(), THICKNESS_FILLED);

	while (true) {
		/*
		amc_message_t sleep_ack = {0};
		amc_message_await("com.axle.core", &sleep_ack);
		*/
	}

	//printf("Received ack from awm: %s\n", receive_ack.data);
while (true) {}

	//ca_layer dummy_layer;
	//dummy_layer.size = _screen.resolution;
	//dummy_layer.raw = (uint8_t*)_screen.physbase;
	//dummy_layer.alpha = 1.0;

	//draw_rect(_screen.vmem, rect_make(point_make(200, 300), size_make(600, 400)), color_red(), THICKNESS_FILLED);
	//draw_rect(&dummy_layer, rect_make(point_zero(), _screen.resolution), color_white(), THICKNESS_FILLED);
	//draw_rect(&dummy_layer, rect_make(point_make(200, 300), size_make(600, 400)), color_red(), THICKNESS_FILLED);

	Point origin = point_make(40, 40);
	Point cursor = origin;
	Size font_size = size_make(40, 64);
	Size padding = size_make(1, 20);
	while (true) {
		amc_message_t msg = {0};
		amc_message_await("com.axle.tty", &msg);

		/*
		Color draw_color = color_green();
		if (!strcmp(msg.source, "com.axle.kb_driver")) {
			draw_color = color_blue();
		}
		draw_char(&dummy_layer, msg.data[0], cursor.x, cursor.y, draw_color, font_size);

		cursor.x += font_size.width + padding.width;
		if (cursor.x >= _screen.resolution.width) {
			cursor.x = 0;
			cursor.y += font_size.height + padding.height;
		}

		if (cursor.y >= _screen.resolution.height) {
			draw_rect(&dummy_layer, rect_make(point_zero(), _screen.resolution), color_white(), THICKNESS_FILLED);
			cursor = origin;
		}
		*/
	}

	return 0;
}

