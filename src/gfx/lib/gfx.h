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

typedef struct __attribute__((packed)) {
	unsigned short di, si, bp, sp, bx, dx, cx, ax;
	unsigned short gs, fs, es, ds, eflags;
} regs16_t;

typedef struct window Window;
typedef struct screen_t {
	Window* window; //root window
	uint16_t depth; //bits per pixel
	uint8_t bpp; //bytes per pixel
	Size resolution;
	uint32_t* physbase; //address of beginning of framebuffer
	volatile int finished_drawing; //are we currently rendering a frame?
	ca_layer* vmem; //raw framebuffer pushed to screen
	Size default_font_size; //recommended font size for screen resolution
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

//fill double buffer with a given Color
void fill_screen(Screen* screen, Color color);
//copy all double buffer data to real screen
void write_screen(Screen* screen);
//copy 'region' from double buffer to real screen
void write_screen_region(Rect region);

void draw_boot_background();

void gfx_init(void* mboot_ptr);
void process_gfx_switch(Screen* screen, int new_depth);
void set_gfx_depth(uint32_t depth);
int gfx_depth();
int gfx_bpp();
Screen* gfx_screen();

Vec2d vec2d(double x, float y);

__attribute__((always_inline))
inline void putpixel(ca_layer* layer, int x, int y, Color color) {
	//don't attempt writing a pixel outside of screen bounds
	//if (x < 0 || y < 0 || x >= layer->size.width || y >= layer->size.height) return;

	int bpp = gfx_bpp();
	bool write_directly = !(layer);
	int offset = (x * bpp) + (y * layer->size.width * bpp);
	if (write_directly) {
		offset = (x * bpp) + (y * gfx_screen()->resolution.width * bpp);

		int bank = offset / BANK_SIZE;
		int bank_offset = offset % BANK_SIZE;
		vbe_set_bank(bank);
		uint8_t* vmem = (uint8_t*)VBE_DISPI_LFB_PHYSICAL_ADDRESS;
		vmem[bank_offset + 0] = color.val[0];
		vmem[bank_offset + 1] = color.val[1];
		vmem[bank_offset + 2] = color.val[2];
		return;
	}

	for (int i = 0; i < MIN(3, bpp); i++) {
		//we have to write the pixels in BGR, not RGB
		//layer->raw[offset + i] = color.val[bpp - 1 - i];
		layer->raw[offset + i] = color.val[bpp - 1 - i];
	}
}
__attribute__((always_inline))
inline void addpixel(ca_layer* layer, int x, int y, Color color) {
	//don't attempt writing a pixel outside of screen bounds
	if (x < 0 || y < 0 || x >= layer->size.width || y >= layer->size.height) return;

	int bpp = gfx_bpp();
	int offset = (x * bpp) + (y * layer->size.width * bpp);
	for (int i = 0; i < bpp; i++) {
		//we have to write the pixels in BGR, not RGB
		layer->raw[offset + i] += color.val[bpp - 1 - i];
	}
}

#endif
