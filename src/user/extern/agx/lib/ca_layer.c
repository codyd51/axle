#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ca_layer.h"
#include "rect.h"
#include "../math.h"
#include "../lib/putpixel.h"
#include "../lib/gfx.h"

void layer_teardown(ca_layer* layer) {
	if (!layer) return;

	free(layer->raw);
	/*
	while (layer->clip_rects->count) {
		List_remove_at(layer->clip_rects, 0);
	}
	free(layer->clip_rects);
	*/
	free(layer);
}

ca_layer* create_layer(Size size) {
	ca_layer* ret = (ca_layer*)malloc(sizeof(ca_layer));
	memset(ret, 0, sizeof(ca_layer));
	ret->size = size;
	ret->raw = malloc(size.width * size.height * gfx_bytes_per_pixel());
	ret->alpha = 1.0;
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
	int bpp = gfx_bytes_per_pixel();
	//for every pixel in dest, calculate what the pixel should be based on 
	//dest's pixel, src's pixel, and the alpha
	
	if (src->alpha == 0.5) {
		blit_layer_alpha_fast(dest, src, dest_frame, src_frame);
		return;
	}

	//offset into dest that we start writing
	uint8_t* dest_row_start = dest->raw + (rect_min_y(dest_frame) * dest->size.width * bpp) + (rect_min_x(dest_frame) * bpp);

	//data from source to write to dest
	uint8_t* row_start = src->raw + (rect_min_y(src_frame) * src->size.width * bpp) + rect_min_x(src_frame) * bpp;
	
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

			dest_px += bpp;
			row_px += bpp;
		}

		//next iteration, start at the next row
		dest_row_start += (dest->size.width * bpp);
		row_start += (src->size.width * bpp);
	}
}

void blit_layer_filled(ca_layer* dest, ca_layer* src, Rect dest_frame, Rect src_frame) {
	int bpp = gfx_bytes_per_pixel();
	//copy row by row
	
	//offset into dest that we start writing
	uint8_t* dest_row_start = dest->raw + (rect_min_y(dest_frame) * dest->size.width * bpp) + (rect_min_x(dest_frame) * bpp);

	//data from source to write to dest
	uint8_t* row_start = src->raw + (rect_min_y(src_frame) * src->size.width * bpp) + rect_min_x(src_frame) * bpp;

	int transferabble_rows = src_frame.size.height;
	int overhang = rect_max_y(src_frame) - src->size.height;
	if (overhang > 0) {
		transferabble_rows -= overhang;
	}
	//copy height - y origin rows
	int total_px_in_layer = (uint32_t)(dest->size.width * dest->size.height * bpp);
	int dest_max_y = rect_max_y(dest_frame);
	for (int i = 0; i < transferabble_rows; i++) {
		if (i >= dest_max_y) break;

		//figure out how many px we can actually transfer over,
		//in case src_frame exceeds dest
		int offset = (uint32_t)dest_row_start - (uint32_t)dest->raw;
		if (offset >= total_px_in_layer) {
			break;
		}

		// blit_layer should handle bounding the provided frames so we never write to memory outside the layers,
		// regardless of the frames passed.
		int transferabble_px = MIN(src_frame.size.width, dest_frame.size.width) * bpp;
		memcpy(dest_row_start, row_start, transferabble_px);

		dest_row_start += (dest->size.width * bpp);
		row_start += (src->size.width * bpp);
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

/*
void layer_add_clip_context(ca_layer* layer, ca_layer* clip_subject, Rect added_clip_rect) {
	Rect cur_rect;

	//check each existing clip rect and see if it overlaps with new one
	for (uint32_t i = 0; i < layer->clip_rects->count; ) {
		clip_context_t* context = List_get_at(layer->clip_rects, i);
		cur_rect = context->clip_rect;
		if (!rect_intersects(cur_rect, added_clip_rect)) {
			//no intersection, nothing to do here
			i++;
			continue;
		}
		
		//current rect in clip list *does* intersect with new rect in clip list
		//we must split these into smaller clip regions :}
		
		//remove original and replace with clips
		clip_context_t* old_context = List_remove_at(layer->clip_rects, i);

		Rect pre_split_rect = cur_rect;
		List* split_list = Rect_split(cur_rect, added_clip_rect);

		while(split_list->count) {
            Rect* cur_rect = (Rect*)List_remove_at(split_list, 0); //Pull from A

			clip_context_t* split_context = kmalloc(sizeof(clip_context_t));
			split_context->source_layer = context->source_layer;

			split_context->clip_rect.origin = cur_rect->origin;
			split_context->clip_rect.size = cur_rect->size;

			int diffx = cur_rect->origin.x - pre_split_rect.origin.x;
			split_context->local_origin.x += diffx;

			int diffy = cur_rect->origin.y - pre_split_rect.origin.y;
			split_context->local_origin.y += diffy;
			/*
			//split_context->local_origin = cur_rect->origin;
			int diffx = cur_rect->origin.x - split_context->local_origin.x;
			split_context->local_origin.x += diffx;

			int diffy = cur_rect->origin.y - split_context->local_origin.y;
			split_context->local_origin.y += diffy;
			//split_context->local_origin 
			//**

            List_add(layer->clip_rects, split_context); //Push to B
        }
		kfree(split_list);

		kfree(old_context);

		//we've removed an item from the list and added to it
		//therefore, we need to start from the beginning of the list again
		//note, this means the loop will exit once nothing in the clip list overlaps
		i = 0;
	}

	//we've guaranteed nothing in the clip list overlaps
	//finally, insert new clip rect
	clip_context_t* new_context = kmalloc(sizeof(clip_context_t));
	new_context->source_layer = clip_subject;

	new_context->clip_rect.origin = added_clip_rect.origin;
	new_context->clip_rect.size = added_clip_rect.size;

	//new_context->local_origin = added_clip_rect.origin;
	/*
	if (added_clip_rect.origin.x < 0 || added_clip_rect.origin.y < 0) {
		new_context->local_origin = added_clip_rect.origin;
	}
	else {
	**
		new_context->local_origin = point_zero();
	//}
	List_add(layer->clip_rects, new_context);
}

void layer_clear_clip_rects(ca_layer* layer) {
	while (layer->clip_rects->count) {
		clip_context_t* removed;
		if ((removed = List_remove_at(layer->clip_rects, 0))) {
			kfree(removed);
		}
	}
}
*/
