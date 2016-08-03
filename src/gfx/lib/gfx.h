#ifndef GFX_H
#define GFX_H

#include <std/common.h>
#include <std/timer.h>
#include <gfx/font/font.h>

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
	Font* font;
} Screen;

extern void int32(unsigned char intnum, regs16_t* regs);

void switch_to_text();

void gfx_teardown(Screen* screen);

void vga_boot_screen(Screen* screen);

void putpixel(Screen* screen, int x, int y, Color color);

void fill_screen(Screen* screen, Color color);

void write_screen(Screen* screen);

#endif
