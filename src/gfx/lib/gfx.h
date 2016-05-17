#ifndef GFX_H
#define GFX_H

#include <std/common.h>

typedef struct __attribute__((packed)) {
	unsigned short di, si, bp, sp, bx, dx, cx, ax;
	unsigned short gs, fs, es, ds, eflags;
} regs16_t;

typedef struct {
	uint16_t width;
	uint16_t height;
	uint16_t pitch;
	uint16_t depth;
	uint16_t pixelwidth;
} screen_t;

extern void int32(unsigned char intnum, regs16_t* regs);

screen_t* get_gfx_screen();
void switch_to_text();
void gfx_test();
void boot_screen();

void putpixel(screen_t* screen, int x, int y, int color);

#endif
