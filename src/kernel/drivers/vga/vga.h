#ifndef VGA_H
#define VGA_H

#include <gfx/lib/gfx.h>
#include <gfx/lib/color.h>

#define VRAM_START 0xA0000

Screen* switch_to_vga();

#endif
