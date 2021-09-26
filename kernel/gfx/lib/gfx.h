#ifndef GFX_H
#define GFX_H

typedef void (*event_handler)(void* obj, void* context);

#include <kernel/multiboot.h>
#include <std/common.h>
#include <std/math.h>
#include <std/timer.h>
#include <gfx/font/font.h>
#include <gfx/lib/surface.h>

#include "window.h"
#include "screen.h"
#include "button.h"
#include "putpixel.h"
#include "rect.h"
#include "view.h"

typedef struct __attribute__((packed)) {
	unsigned short di, si, bp, sp, bx, dx, cx, ax;
	unsigned short gs, fs, es, ds, eflags;
} regs16_t;

typedef struct window Window;
typedef struct Vec2d {
	double x;
	double y;
} Vec2d;

void draw_boot_background();
void display_boot_screen();

Screen* gfx_init(void);
Screen* gfx_screen();
int gfx_bytes_per_pixel();
int gfx_bits_per_pixel();

void gfx_terminal_putchar(char ch);
void gfx_terminal_puts(const char* str);

Vec2d vec2d(double x, float y);

#endif
