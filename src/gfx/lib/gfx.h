#ifndef GFX_H
#define GFX_H

#include <std/common.h>
#include <std/timer.h>
#include <gfx/font/font.h>

#include "rect.h"
#include "view.h"

typedef struct __attribute__((packed)) {
	unsigned short di, si, bp, sp, bx, dx, cx, ax;
	unsigned short gs, fs, es, ds, eflags;
} regs16_t;

typedef struct screen_t {
	Window* window;
	uint16_t pitch;
	uint16_t depth;
	uint16_t pixelwidth;
	uint8_t* vmem;
	uint8_t* physbase;
	timer_callback callback;
	int finished_drawing;
} Screen;

extern void int32(unsigned char intnum, regs16_t* regs);

void switch_to_text();
void gfx_teardown(Screen* screen);
void vga_boot_screen(Screen* screen);

void fill_screen(Screen* screen, Color color);
void write_screen(Screen* screen);

#define VESA_DEPTH 24
#define VGA_DEPTH 8 
__attribute__((always_inline)) void inline putpixel(Screen* screen, int x, int y, Color color) {
	//don't attempt writing a pixel outside of screen bounds
	if (x >= screen->window->size.width || y >= screen->window->size.height) return;

	if (screen->depth == VGA_DEPTH) {
		//VGA mode
		uint16_t loc = ((y * screen->window->size.width) + x);
		screen->vmem[loc] = color.val[0];
	}
	else if (screen->depth == VESA_DEPTH) {
		//VESA mode
		static int bpp = 24 / 8;
		int offset = x * bpp + y * screen->window->size.width * bpp;
		//we have to write the pixels in BGR, not RGB
		screen->vmem[offset + 0] = color.val[2];
		screen->vmem[offset + 1] = color.val[1];
		screen->vmem[offset + 2] = color.val[0];
	}
}

#endif
