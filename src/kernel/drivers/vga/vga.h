#ifndef VGA_H
#define VGA_H

#include <gfx/lib/gfx.h>
#include <gfx/lib/color.h>

#define VRAM_START 0xA0000
#define VGA_DEPTH 8 

Screen* switch_to_vga();

void putpixel_vga(Screen* screen, int x, int y, Color color);

#endif
