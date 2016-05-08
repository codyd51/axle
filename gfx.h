#include "common.h"

typedef struct __attribute__((packed)) {
	unsigned short di, si, bp, sp, bx, dx, cx, ax;
	unsigned short gs, fs, es, ds, eflags;
} regs16_t;

typedef struct {
	u16int width;
	u16int height;
	u16int pitch;
	u16int depth;
	u16int pixelwidth;
} screen_t;

screen_t* screen;

extern void int32(unsigned char intnum, regs16_t* regs);

void switch_to_gfx();
void switch_to_text();
void gfx_test();
