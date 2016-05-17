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

typedef struct coordinate {
	int x;
	int y;
} coordinate;

typedef struct size {
	int w;
	int h;
} size;

typedef struct rect {
	coordinate* origin;
	size* size;
} rect;

typedef struct line {
	coordinate* p1;
	coordinate* p2;
} line;

typedef struct circle {
	coordinate center;
	int radius;
} circle;

typedef struct triangle {
	coordinate p1;
	coordinate p2;
	coordinate p3;
} triangle;

extern void int32(unsigned char intnum, regs16_t* regs);

screen_t* get_gfx_screen();
void switch_to_text();
void gfx_test();

void putpixel(screen_t* screen, int x, int y, int color);

#endif
