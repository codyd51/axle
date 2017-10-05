#ifndef SURFACE_H
#define SURFACE_H

#include <stdint.h>
#include <gfx/lib/shapes.h>

typedef struct surface {
	uint8_t* base_address;
	uint32_t size;
	uint32_t width;
	uint32_t height;
	uint8_t bpp;
	uint8_t* kernel_base;
} Surface;

Surface* surface_make(uint32_t width, uint32_t height, uint32_t dest_pid);

#endif
