#ifndef GFX_H
#define GFX_H

typedef void (*event_handler)(void* obj, void* context);

#include <kernel/multiboot.h>
#include <std/common.h>
#include <std/math.h>
#include <std/timer.h>

#include "screen.h"
#include "putpixel.h"
#include "rect.h"

typedef struct __attribute__((packed)) {
	unsigned short di, si, bp, sp, bx, dx, cx, ax;
	unsigned short gs, fs, es, ds, eflags;
} regs16_t;

typedef struct window Window;

void draw_boot_background();
void display_boot_screen();

Screen* gfx_init(void);
Screen* gfx_screen();
int gfx_bytes_per_pixel();
int gfx_bits_per_pixel();

void gfx_terminal_putchar(char ch);
void gfx_terminal_puts(const char* str);

// PT(07/07/22): New kernel graphics interface
Size kernel_gfx_screen_size(void);
void kernel_gfx_fill_rect(Rect r, Color color);
void kernel_gfx_fill_screen(Color color);
Point kernel_gfx_draw_string(uint8_t* dest, char* str, Point origin, Color color, Size font_size);
void kernel_gfx_draw_char(uint8_t* dest, char ch, int x, int y, Color color, Size font_size);
void kernel_gfx_putpixel(uint8_t* dest, int x, int y, Color color);
void kernel_gfx_set_line_rendered_string_cursor(Point new_cursor_pos);
void kernel_gfx_write_line_rendered_string(char* str);
void kernel_gfx_write_line_rendered_string_ex(char* str, bool higher_half);

#endif
