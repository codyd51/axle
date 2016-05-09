#include "gfx.h"
#include "std.h"
#include "kernel.h"
#include "timer.h"

#define VRAM_START 0xA0000

screen_t* get_vga_screen() {
	regs16_t regs;
	regs.ax = 0x0013;
	int32(0x10, &regs);

	screen_t* screen = (screen_t*)malloc(sizeof(screen_t));
	screen->width = 320;
	screen->height = 200;
	screen->depth = 256;
	return screen;
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

void putpixel(screen_t* screen, int x, int y, int color) {
	unsigned loc = VRAM_START + (y * screen->width) + x;
	memset(loc, color, 1);
}

void fill_screen(screen_t* screen, int color) {
	memset((char*)VRAM_START, color, (screen->width * screen->height));
}

//calling putpixel directly will always be slow
//since we have to calculate where the pixel goes for every single pixel we place
//suffix _slow, replace this with faster function in future
void fillrect_slow(screen_t* screen, int x, int y, int w, int h, int color) {
	for (int i = y; i < h; i++) {
		for (int j = x; j < w; j++) {
			putpixel(screen, j, i, color);
		}
	}
}

void hline_slow(screen_t* screen, int x, int y, int w, int color) {
	for (; x < w; x++) {
	       putpixel(screen, x, y, color);
	}
}	

void vline_slow(screen_t* screen, int x, int y, int h, int color) {
	for (; y < h; y++) {
		putpixel(screen, x, y, color);
	}
}

void gfx_test() {
	screen_t* screen = get_vga_screen();
	fill_screen(screen, 1);

	fillrect_slow(screen, 50, 5, 100, 10, 6);
	hline_slow(screen, 80, 30, 200, 7);
	vline_slow(screen, 25, 5, 150, 9);

	sleep(3000);
	//getchar();
	switch_to_text();
}

