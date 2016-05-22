#ifndef GFX_H
#define GFX_H

#include <std/common.h>
#include "std/timer.h"

#include "view.h"

typedef struct __attribute__((packed)) {
	unsigned short di, si, bp, sp, bx, dx, cx, ax;
	unsigned short gs, fs, es, ds, eflags;
} regs16_t;

typedef struct {
	window window;
	uint16_t pitch;
	uint16_t depth;
	uint16_t pixelwidth;
	uint8_t* vmem;
	uint8_t* physbase;
	timer_callback callback;
} Screen;

extern void int32(unsigned char intnum, regs16_t* regs);

Screen* switch_to_vga();
void switch_to_text();

void boot_screen();

void putpixel(Screen* screen, int x, int y, int color);

#endif
