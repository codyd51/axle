#include "gfx.h"
#include "std.h"
#include "kernel.h"

#define VRAM_START 0xA0000

void int32_test() {
	int y;
	regs16_t regs;

	//switch to 320x200x256 gfx mode
	regs.ax = 0x0013;
	int32(0x10, &regs);

	//full screen with blue color
	memset((char*)0xA0000, 1, (320*200));

	//draw horizontal line from 100,80 to 100,240 in different colors
	for (y = 0; y < 200; y++) {
		memset((char*)0xA0000 + (y*320+80), y, 160);
	}

	//wait for key
	regs.ax = 0x0000;
	int32(0x16, &regs);

	//switch to 80x25x16 text mode
	regs.ax = 0x0003;
	int32(0x10, &regs);
}

void switch_to_gfx() {
	regs16_t regs;
	regs.ax = 0x0013;
	int32(0x10, &regs);

	screen = (screen_t*)malloc(sizeof(screen_t));
	screen->width = 320;
	screen->height = 200;
	screen->depth = 256;

}

void switch_to_text() {
	regs16_t regs;
	regs.ax = 0x0003;
	int32(0x10, &regs);
}

void wait_keypress() {
	regs16_t regs;
	regs.ax = 0x0000;
	int32(0x10, &regs);
}

void putpixel(int x, int y, int color) {
	unsigned loc = VRAM_START + (y * screen->width) + x;
	memset(loc, color, 1);
}

void fill_screen(int color) {
	memset((char*)VRAM_START, color, (screen->width * screen->height));
}

void gfx_test() {
	switch_to_gfx();
	fill_screen(1);
	getchar();
	switch_to_text();
}

