#ifndef GFX_H
#define GFX_H

#include <std/common.h>
#include <std/timer.h>

typedef void (*event_handler)(void* obj, void* context);

#include "rect.h"
#include "view.h"
#include "button.h"
#include <gfx/font/font.h>
#include <std/math.h>
#include <kernel/drivers/vbe/vbe.h>
#include <gfx/lib/surface.h>
#include <kernel/multiboot.h>

typedef struct __attribute__((packed)) {
	unsigned short di, si, bp, sp, bx, dx, cx, ax;
	unsigned short gs, fs, es, ds, eflags;
} regs16_t;

typedef struct window Window;
typedef struct screen_t {
	uint32_t* physbase; //address of beginning of framebuffer
	uint32_t video_memory_size;

	Size resolution;
	uint16_t bits_per_pixel;
	uint8_t bytes_per_pixel;

	Size default_font_size; //recommended font size for screen resolution

	ca_layer* vmem; //raw framebuffer pushed to screen
	Window* window; //root window
	array_m* surfaces;
} Screen;

typedef struct Vec2d {
	double x;
	double y;
} Vec2d;

extern void int32(unsigned char intnum, regs16_t* regs);

Screen* screen_create(Size dimensions, uint32_t* physbase, uint8_t depth);

void gfx_teardown(Screen* screen);
void vga_boot_screen(Screen* screen);

//fill double buffer with a given Color
void fill_screen(Screen* screen, Color color);
//copy all double buffer data to real screen
void write_screen(Screen* screen);
//copy 'region' from double buffer to real screen
void write_screen_region(Rect region);

void draw_boot_background();
void display_boot_screen();

Screen* gfx_init(void);
Screen* gfx_screen();
int gfx_bytes_per_pixel();
int gfx_bits_per_pixel();

Vec2d vec2d(double x, float y);

__attribute__((always_inline))
inline void putpixel_alpha(ca_layer* layer, int x, int y, Color color, int alpha) {
	//don't attempt writing a pixel outside of screen bounds
	if (x < 0 || y < 0 || x >= layer->size.width || y >= layer->size.height) return;
	alpha = MAX(alpha, 0);
	alpha = MIN(alpha, 255);

	Screen* screen = gfx_screen();
	bool write_directly = !(layer);

	int offset = (x * screen->bytes_per_pixel) + (y * layer->size.width * screen->bytes_per_pixel);
	
	/*
	if (depth == 4 || write_directly) {
		offset = (x * bpp) + (y * gfx_screen()->resolution.width * bpp);

		int bank = offset / BANK_SIZE;
		int bank_offset = offset % BANK_SIZE;
		vbe_set_bank(bank);

		uint8_t* vmem = (uint8_t*)VBE_DISPI_LFB_PHYSICAL_ADDRESS;

		if (depth == 4) {
			vmem[bank_offset] = 3;
		}
		else if (write_directly) {
			vmem[bank_offset + 0] = color.val[0];
			vmem[bank_offset + 1] = color.val[1];
			vmem[bank_offset + 2] = color.val[2];
		}
		return;
	}
	*/
	for (int i = 0; i < 3; i++) {
		//((alpha*(src-dest))>>8)+dest
		//we have to write the pixels in BGR, not RGB
		//therefore, flip color order when getting src
		int src = color.val[screen->bytes_per_pixel - 1 - i];
		int dst = layer->raw[offset + i];
		int composited = ((alpha * (src - dst)) >> 8) + dst;
		layer->raw[offset + i] = composited;
	}
}

__attribute__((always_inline))
inline void putpixel(ca_layer* layer, int x, int y, Color color) {
	//don't attempt writing a pixel outside of screen bounds
	if (x < 0 || y < 0 || x >= layer->size.width || y >= layer->size.height) return;

	Screen* screen = gfx_screen();
	if (!screen) panic("Cannot call putpixel() without a screen");

	bool write_directly = !(layer);
	uint32_t offset = (x * screen->bytes_per_pixel) + (y * layer->size.width * screen->bytes_per_pixel);

	for (uint32_t i = 0; i < 3; i++) {
		//we have to write the pixels in BGR, not RGB
		//layer->raw[offset + i] = color.val[bpp - 1 - i];
		layer->raw[offset + i] = color.val[i];
	}
}
__attribute__((always_inline))
inline void addpixel(ca_layer* layer, int x, int y, Color color) {
	//don't attempt writing a pixel outside of screen bounds
	if (x < 0 || y < 0 || x >= layer->size.width || y >= layer->size.height) return;

	Screen* screen = gfx_screen();
	if (!screen) panic("Cannot call putpixel() without a screen");

	int bytes_per_pixel = screen->bytes_per_pixel;
	int offset = (x * bytes_per_pixel) + (y * layer->size.width * bytes_per_pixel);
	for (int i = 0; i < bytes_per_pixel; i++) {
		//we have to write the pixels in BGR, not RGB
		layer->raw[offset + i] += color.val[bytes_per_pixel - 1 - i];
	}
}

#endif
