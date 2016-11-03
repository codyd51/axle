#ifndef GFX_H
#define GFX_H

#include <std/common.h>
#include <std/timer.h>
#include <std/list.h>

#include "rect.h"
#include "view.h"
#include <gfx/font/font.h>

typedef struct __attribute__((packed)) {
	unsigned short di, si, bp, sp, bx, dx, cx, ax;
	unsigned short gs, fs, es, ds, eflags;
} regs16_t;

typedef struct screen_t {
	Window* window; //root window
	uint16_t pitch; //redundant?
	uint16_t depth; //bits per pixel
	uint8_t bpp; //bytes per pixel
	uint16_t pixelwidth; //redundant?
	uint8_t* physbase; //address of beginning of framebuffer
	volatile int finished_drawing; //are we currently rendering a frame?
} Screen;

typedef struct Vec2d {
	double x;
	double y;
} Vec2d;

extern void int32(unsigned char intnum, regs16_t* regs);

Screen* screen_create(Size dimensions, uint32_t* physbase, uint8_t depth);

void switch_to_text();
void gfx_teardown(Screen* screen);
void vga_boot_screen(Screen* screen);

void fill_screen(Screen* screen, Color color);
void write_screen(Screen* screen);

void process_gfx_switch(int new_depth);
int gfx_depth();
int gfx_bpp();

Vec2d vec2d(double x, float y);

#define VESA_DEPTH 24
#define VGA_DEPTH 8
__attribute__((always_inline))
inline void putpixel(ca_layer* layer, int x, int y, Color color) {
	//don't attempt writing a pixel outside of screen bounds
	if (x < 0 || y < 0 || x >= layer->size.width || y >= layer->size.height) return;

	int depth = gfx_depth();
	if (depth == VGA_DEPTH) {
		//VGA mode
		uint16_t loc = ((y * layer->size.width * gfx_bpp()) + (x * gfx_bpp()));
		layer->raw[loc] = color.val[0];
	}
	else {
		//VESA mode
		int offset = (x * gfx_bpp()) + (y * layer->size.width * gfx_bpp());
		//we have to write the pixels in BGR, not RGB
		layer->raw[offset + 0] = color.val[2];
		layer->raw[offset + 1] = color.val[1];
		layer->raw[offset + 2] = color.val[0];
	}
}
__attribute__((always_inline))
inline void addpixel(ca_layer* layer, int x, int y, Color color) {
	//don't attempt writing a pixel outside of screen bounds
	if (x < 0 || y < 0 || x >= layer->size.width || y >= layer->size.height) return;

	int depth = gfx_depth();
	if (depth == VGA_DEPTH) {
		//VGA mode
		uint16_t loc = ((y * layer->size.width) + x);
		layer->raw[loc] += color.val[0];
	}
	else {
		//VESA mode
		int bpp = 24 / 8;
		int offset = x * bpp + y * layer->size.width * bpp;
		//we have to write the pixels in BGR, not RGB
		layer->raw[offset + 0] += color.val[0];
		layer->raw[offset + 1] += color.val[1];
		layer->raw[offset + 2] += color.val[2];
	}
}

#endif
