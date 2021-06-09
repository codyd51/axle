#ifndef GFX_SCREEN_H
#define GFX_SCREEN_H

#include "size.h"
#include "ca_layer.h"
#include "color.h"

typedef struct window Window;

// Wraps up some helpful info about the framebuffer
// These days, awm is the only one to deal with this
typedef struct screen_t {
	uint32_t video_memory_size;

	Size resolution;
	uint16_t bits_per_pixel;
	uint8_t bytes_per_pixel;

	ca_layer* vmem; // Buffer we composite to
	ca_layer* pmem; // Memory-mapped video memory
} Screen;

//fill double buffer with a given Color
void fill_screen(Screen* screen, Color color);
//copy all double buffer data to real screen
void write_screen(Screen* screen);
//copy 'region' from double buffer to real screen
void write_screen_region(Rect region);

#endif