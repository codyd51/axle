#include "ca_layer.h"
#include <std/kheap.h>
#include "gfx.h"
#include "rect.h"
#include <std/math.h>
#include <std/memory.h>

void layer_teardown(ca_layer* layer) {
	if (!layer) return;

	kfree(layer->raw);
	while (layer->clip_rects->count) {
		List_remove_at(layer->clip_rects, 0);
	}
	kfree(layer->clip_rects);
	kfree(layer);
}

ca_layer* create_layer(Size size) {
	ca_layer* ret = (ca_layer*)kmalloc(sizeof(ca_layer));
	ret->size = size;

	ret->raw = kmalloc(size.width * size.height * gfx_bytes_per_pixel());

	ret->alpha = 1.0;
	ret->clip_rects = List_new();
	return ret;
}

void blit_layer_alpha_fast(ca_layer* dest, ca_layer* src, Rect dest_frame, Rect src_frame) {
	//for every pixel in dest, calculate what the pixel should be based on 
	//dest's pixel, src's pixel, and the alpha
	//
	//offset into dest that we start writing
	uint8_t* dest_row_start = dest->raw + (rect_min_y(dest_frame) * dest->size.width * gfx_bytes_per_pixel()) + (rect_min_x(dest_frame) * gfx_bytes_per_pixel());

	//data from source to write to dest
	uint8_t* row_start = src->raw + (rect_min_y(src_frame) * src->size.width * gfx_bytes_per_pixel()) + rect_min_x(src_frame) * gfx_bytes_per_pixel();
	
	for (int i = 0; i < src_frame.size.height; i++) {
		uint8_t* dest_px = dest_row_start;
		uint8_t* row_px = row_start;

		for (int j = 0; j < src_frame.size.width; j++) {
			//R
			*dest_px = (*dest_px + *row_px++) / 2;
			dest_px++;
			//G
			*dest_px = (*dest_px + *row_px++) / 2;
			dest_px++;
			//B
			*dest_px = (*dest_px + *row_px++) / 2;
			dest_px++;
		}

		//next iteration, start at the next row
		dest_row_start += (dest->size.width * gfx_bytes_per_pixel());
		row_start += (src->size.width * gfx_bytes_per_pixel());
	}
}

void blit_layer_alpha(ca_layer* dest, ca_layer* src, Rect dest_frame, Rect src_frame) {
	//for every pixel in dest, calculate what the pixel should be based on 
	//dest's pixel, src's pixel, and the alpha
	
	if (src->alpha == 0.5) {
		blit_layer_alpha_fast(dest, src, dest_frame, src_frame);
		return;
	}

	//offset into dest that we start writing
	uint8_t* dest_row_start = dest->raw + (rect_min_y(dest_frame) * dest->size.width * gfx_bytes_per_pixel()) + (rect_min_x(dest_frame) * gfx_bytes_per_pixel());

	//data from source to write to dest
	uint8_t* row_start = src->raw + (rect_min_y(src_frame) * src->size.width * gfx_bytes_per_pixel()) + rect_min_x(src_frame) * gfx_bytes_per_pixel();
	
	int alpha = (1 - src->alpha) * 255;
	for (int i = 0; i < src_frame.size.height; i++) {
		uint8_t* dest_px = dest_row_start;
		uint8_t* row_px = row_start;

		for (int j = 0; j < src_frame.size.width; j++) {
			uint32_t* wide_dest = (uint32_t*)dest_px;
			uint32_t* wide_row = (uint32_t*)row_px;

			uint32_t rb = *wide_row & 0xFF00FF;
			uint32_t g = *wide_row & 0x00FF00;
			rb += ((*wide_dest & 0xFF00FF) - rb) * alpha >> 8;
			g += ((*wide_dest & 0x00FF00) - g) * alpha >> 8;
			*wide_dest = (rb & 0xFF00FF) | (g & 0x00FF00) | (*wide_dest & 0xFF000000);

			dest_px += 3;
			row_px += 3;
		}

		//next iteration, start at the next row
		dest_row_start += (dest->size.width * gfx_bytes_per_pixel());
		row_start += (src->size.width * gfx_bytes_per_pixel());
	}
}

void blit_layer_filled(ca_layer* dest, ca_layer* src, Rect dest_frame, Rect src_frame) {
	//copy row by row
	
	//offset into dest that we start writing
	uint8_t* dest_row_start = dest->raw + (rect_min_y(dest_frame) * dest->size.width * gfx_bytes_per_pixel()) + (rect_min_x(dest_frame) * gfx_bytes_per_pixel());

	//data from source to write to dest
	uint8_t* row_start = src->raw + (rect_min_y(src_frame) * src->size.width * gfx_bytes_per_pixel()) + rect_min_x(src_frame) * gfx_bytes_per_pixel();

	int transferabble_rows = src_frame.size.height;
	int overhang = rect_max_y(src_frame) - src->size.height;
	if (overhang > 0) {
		transferabble_rows -= overhang;
	}
	//copy height - y origin rows
	for (int i = 0; i < transferabble_rows; i++) {
		if (i >= rect_max_y(dest_frame)) break;

		//figure out how many px we can actually transfer over,
		//in case src_frame exceeds dest
		int offset = (uint32_t)dest_row_start - (uint32_t)dest->raw;
		int total_px_in_layer = (uint32_t)(dest->size.width * dest->size.height * gfx_bytes_per_pixel());
		if (offset >= total_px_in_layer) {
			break;
		}

		// blit_layer should handle bounding the provided frames so we never write to memory outside the layers,
		// regardless of the frames passed.
		int transferabble_px = src_frame.size.width * gfx_bytes_per_pixel();
		memcpy(dest_row_start, row_start, transferabble_px);

		dest_row_start += (dest->size.width * gfx_bytes_per_pixel());
		row_start += (src->size.width * gfx_bytes_per_pixel());
	}
}

void blit_layer(ca_layer* dest, ca_layer* src, Rect dest_frame, Rect src_frame) {
	//make sure we don't write outside dest's frame
	rect_min_x(dest_frame) = MAX(0, rect_min_x(dest_frame));
	rect_min_y(dest_frame) = MAX(0, rect_min_y(dest_frame));
	dest_frame.size.width = MIN(dest_frame.size.width, dest->size.width);
	dest_frame.size.height = MIN(dest_frame.size.height, dest->size.height);

	rect_min_x(src_frame) = MAX(0, rect_min_x(src_frame));
	rect_min_y(src_frame) = MAX(0, rect_min_y(src_frame));
	src_frame.size.width = MIN(src_frame.size.width, src->size.width);
	src_frame.size.height = MIN(src_frame.size.height, src->size.height);

	//clip src_frame within dest
	if (src_frame.size.width + rect_min_x(dest_frame) >= dest->size.width) {
		int overhang = src_frame.size.width + rect_min_x(dest_frame) - dest->size.width;
		src_frame.size.width -= overhang;
	}
	if (src_frame.size.height + rect_min_y(dest_frame) >= dest->size.height) {
		int overhang = src_frame.size.height + rect_min_y(dest_frame) - dest->size.height;
		src_frame.size.height -= overhang;
	}

	if (src->alpha >= 1.0) {
		//best case, we can just copy rows directly from src to dest
		blit_layer_filled(dest, src, dest_frame, src_frame);
	}
	else if (src->alpha <= 0) {
		//do nothing
		//return;
	}
	else {
		blit_layer_alpha(dest, src, dest_frame, src_frame);
	}
}

ca_layer* layer_snapshot(ca_layer* src, Rect frame) {
	//clip frame
	rect_min_x(frame) = MAX(0, rect_min_x(frame));
	rect_min_y(frame) = MAX(0, rect_min_y(frame));
	if (rect_max_x(frame) >= src->size.width) {
		double overhang = rect_max_x(frame) - src->size.width;
		frame.size.width -= overhang;
	}
	if (rect_max_y(frame) >= src->size.height) {
		double overhang = rect_max_y(frame) - src->size.height;
		frame.size.height -= overhang;
	}

	ca_layer* snapshot = create_layer(frame.size);

	//pointer to current row of snapshot to write to
	uint8_t* snapshot_row = snapshot->raw;
	//pointer to start of row currently writing to snapshot
	uint8_t* row_start = src->raw + (rect_min_y(frame) * src->size.width * gfx_bytes_per_pixel()) + (rect_min_x(frame) * gfx_bytes_per_pixel());

	//copy row by row
	for (int i = 0; i < frame.size.height; i++) {
		memcpy(snapshot_row, row_start, frame.size.width * gfx_bytes_per_pixel());

		snapshot_row += (snapshot->size.width * gfx_bytes_per_pixel());
		row_start += (src->size.width * gfx_bytes_per_pixel());
	}

	return snapshot;
}

void layer_add_clip_context(ca_layer* layer, ca_layer* clip_subject, Rect added_clip_rect) {
    Deprecated();
}

void layer_clear_clip_rects(ca_layer* layer) {
	while (layer->clip_rects->count) {
		clip_context_t* removed;
		if ((removed = List_remove_at(layer->clip_rects, 0))) {
			kfree(removed);
		}
	}
}

