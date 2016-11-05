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
	ret->alpha = 1.0;
	return ret;
}

void blit_layer_alpha(ca_layer* dest, ca_layer* src, Rect copy_frame) {
	//for every pixel in dest, calculate what the pixel should be based on 
	//dest's pixel, src's pixel, and the alpha
		
	//offset into dest that we start writing
	uint8_t* dest_row_start = dest->raw + (rect_min_y(copy_frame) * dest->size.width * gfx_bpp()) + (rect_min_x(copy_frame) * gfx_bpp());
	//data from source to write to dest
	uint8_t* row_start = src->raw;
	
	//multiply by 100 so we can use fixed point math
	int alpha = src->alpha * 100;
	//precalculate inverse alpha
	int inv = 100 - alpha;

	for (int i = 0; i < copy_frame.size.height; i++) {
		uint8_t* dest_px = dest_row_start;
		uint8_t* row_px = row_start;

		for (int j = 0; j < copy_frame.size.width; j++) {
			//R component
			*dest_px = ((*dest_px * alpha) + (*row_px * inv)) / 100;
			dest_px++;
			row_px++;

			//G component
			*dest_px = ((*dest_px * alpha) + (*row_px * inv)) / 100;
			dest_px++;
			row_px++;

			//B component
			*dest_px = ((*dest_px * alpha) + (*row_px * inv)) / 100;
			dest_px++;
			row_px++;
		}

		//next iteration, start at the next row
		dest_row_start += (dest->size.width * gfx_bpp());
		row_start += (src->size.width * gfx_bpp());
	}
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

	if (src->alpha >= 1.0) {
		//best case, we can just copy rows directly from src to dest
		//copy row by row
		
		//offset into dest that we start writing
		uint8_t* dest_row_start = dest->raw + (rect_min_y(copy_frame) * dest->size.width * gfx_bpp()) + (rect_min_x(copy_frame) * gfx_bpp());
		//data from source to write to dest
		uint8_t* row_start = src->raw;
		for (int i = 0; i < copy_frame.size.height; i++) {
			memcpy(dest_row_start, row_start, copy_frame.size.width * gfx_bpp());

			dest_row_start += (dest->size.width * gfx_bpp());
			row_start += (src->size.width * gfx_bpp());
		}
	}
	else if (src->alpha <= 0) {
		//do nothing
		return;
	}
	else {
		blit_layer_alpha(dest, src, copy_frame);
	}
}

