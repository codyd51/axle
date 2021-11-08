#ifndef GFX_SCREEN_H
#define GFX_SCREEN_H

#include "size.h"
#include "ca_layer.h"

typedef struct window Window;

// Root display structure to which all graphics are rendered
// This may only be created via a call to gfx_init()
typedef struct screen_t {
	uintptr_t* physbase; //address of beginning of framebuffer
	uint32_t video_memory_size;

	Size resolution;
	uint16_t bits_per_pixel;
	uint8_t bytes_per_pixel;

	Size default_font_size; //recommended font size for screen resolution

	ca_layer* vmem; //raw framebuffer pushed to screen
	Window* window; //root window
} Screen;

//copy all double buffer data to real screen
void write_screen(Screen* screen);
//copy 'region' from double buffer to real screen
void write_screen_region(Rect region);


#endif