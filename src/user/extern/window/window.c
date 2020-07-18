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

static void _handle_awm_message(amc_message_t awm_msg) {
	ca_layer dummy_layer;
	dummy_layer.size = size_make(1920, 1080);
	dummy_layer.raw = (uint8_t*)_screen.physbase;
	dummy_layer.alpha = 1.0;

	static int i = 0;
	if (!strcmp(awm_msg.data, "redraw")) {
		/*
		draw_rect(&dummy_layer, rect_make(point_make(100, 100), size_make(100, 100)), color_purple(), THICKNESS_FILLED);
		int x = 10 + (rand() % 450);
		int y = 10 + (rand() % 450);
		int w = 10 + (rand() % 450);
		int h = 10 + (rand() % 450);
		int r = (rand() % 255);
		int g = (rand() % 255);
		int b = (rand() % 255);
		draw_rect(&dummy_layer, rect_make(point_make(x, y), size_make(w, h)), color_make(r, g, b), THICKNESS_FILLED);
		*/

		const char* cmd2 = "update_framebuf";
		amc_message_t* draw_framebuf_msg = amc_message_construct(cmd2, strlen(cmd2));
		amc_message_send("com.axle.awm", draw_framebuf_msg);

		/*
		char buf[i+1];
		for (int j = 0; j < i; j++) {
			buf[j] = 'x';
		}
		buf[i]='\0';
		printf("%d: %s\n", i, buf);
		*/
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

static void _handle_tty_message(text_box_t* text_box, amc_message_t tty_msg) {
	Color stdout_color = color_purple();
	for (int i = 0; i < tty_msg.len; i++) {
		char ch = tty_msg.data[i];
		//if ((ch < 'A' || ch > 'z') && ch != '\n') continue;
		_putchar(text_box, ch, color_black());
		/*
		int r = (rand() % 255);
		int g = (rand() % 255);
		int b = (rand() % 255);
		Color color = color_make(r, g, b);
		char buf[32];
		snprintf(&buf, 32, "%d", ch);
		for (int x = 0; x < strlen(buf); x++) {
			_putchar(text_box, buf[x], color);
		}
		_putchar(text_box, ' ', color);
		*/
	}
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
	_screen.physbase = (uint32_t*)framebuffer_addr;
	_screen.bits_per_pixel = 32;
	_screen.bytes_per_pixel = 4;

	ca_layer* dummy_layer = malloc(sizeof(ca_layer));
	memset(dummy_layer, 0, sizeof(dummy_layer));
	dummy_layer->size = size_make(1920, 1080);
	dummy_layer->raw = (uint8_t*)framebuffer_addr;
	dummy_layer->alpha = 1.0;


	text_box_t* text_box = malloc(sizeof(text_box_t));
	memset(text_box, 0, sizeof(text_box_t));
	text_box->layer = dummy_layer;
	text_box->size = size_make(700, 500);
	text_box->origin = point_make(40, 40);
	text_box->cursor_pos = text_box->origin;
	text_box->font_size = size_make(12, 12);
	text_box->font_padding = size_make(2, 2);

	draw_rect(dummy_layer, rect_make(point_zero(), size_make(800, 600)), color_white(), THICKNESS_FILLED);
	draw_rect(dummy_layer, rect_make(point_zero(), size_make(800, 600)), color_black(), 10);

	int i = 0; 
	while (true) {
		i++;
		amc_message_t msg = {0};
		const char* services[] = {"com.axle.awm", "com.axle.tty"};
		amc_message_await_from_services(sizeof(services) / sizeof(services[0]), &services, &msg);

		if (!strcmp(msg.source, "com.axle.awm")) {
			_handle_awm_message(msg);
		}
		else if (!strcmp(msg.source, "com.axle.tty")) {
			//_handle_tty_message(text_box, msg);
		}
		else {
			printf("Unrecognized message: %s\n", msg.source);
			continue;
		}
	}

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

