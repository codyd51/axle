#include "ca_layer.h"
#include <std/kheap.h>
#include "gfx.h"
#include "rect.h"
#include <std/math.h>
#include <std/memory.h>

void layer_teardown(ca_layer* layer) {
	if (!layer) return;

	kfree(layer->raw);
	kfree(layer);
}

ca_layer* create_layer(Size size) {
	ca_layer* ret = (ca_layer*)kmalloc(sizeof(ca_layer));
	ret->size = size;
	ret->raw = (uint8_t*)kmalloc(size.width * size.height * gfx_bpp());
	return ret;
}

void blit_layer(ca_layer* dest, ca_layer* src, Coordinate origin) {
	Rect copy_frame = rect_make(origin, src->size);
	//make sure we don't write outside dest's frame
	rect_min_x(copy_frame) = MAX(0, rect_min_x(copy_frame));
	rect_min_y(copy_frame) = MAX(0, rect_min_y(copy_frame));
	if (rect_max_x(copy_frame) >= dest->size.width) {
		double overhang = rect_max_x(copy_frame) - dest->size.width;
		copy_frame.size.width -= overhang;
	}
	if (rect_max_y(copy_frame) >= dest->size.height) {
		double overhang = rect_max_y(copy_frame) - dest->size.height;
		copy_frame.size.height -= overhang;
	}

	//offset into dest that we start writing
	uint8_t* dest_row_start = dest->raw + (rect_min_y(copy_frame) * dest->size.width * gfx_bpp()) + (rect_min_x(copy_frame) * gfx_bpp());
	//data from source to write to dest
	uint8_t* row_start = src->raw;

	//copy row by row
	for (int i = 0; i < copy_frame.size.height; i++) {
		memcpy(dest_row_start, row_start, copy_frame.size.width * gfx_bpp());

		dest_row_start += (dest->size.width * gfx_bpp());
		row_start += (src->size.width * gfx_bpp());
	}
}

