#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <memory.h>

#include <stdlibadd/assert.h>
#include <agx/lib/putpixel.h>

#include "libimg.h"

#define BMP_MAGIC 0x424d	// 'BM'
typedef struct bmp_file_header {
	uint16_t signature;
	uint32_t size;
	uint32_t reserved;
	uint32_t data_offset;
} __attribute__((packed)) bmp_file_header_t;

typedef struct bmp_info_header {
	uint32_t size;
	uint32_t width;
	uint32_t height;
	uint16_t planes;
	uint16_t bit_count;
	uint32_t compression;
	uint32_t compressed_size;
	uint32_t x_pixels_per_meter;
	uint32_t y_pixels_per_meter;
	uint32_t colors_used;
	uint32_t colors_important;
} __attribute__((packed)) bmp_info_header_t;

image_bmp_t* image_parse_bmp(uint32_t size, uint8_t* data) {
    image_bmp_t* ret = calloc(1, sizeof(image_bmp_t));
	bmp_file_header_t* bmp_header = (bmp_file_header_t*)data;
	bmp_info_header_t* bmp_info = (bmp_info_header_t*)(data + sizeof(bmp_file_header_t));

	uint8_t* pixel_data = (uint8_t*)(data + (bmp_header->data_offset));
	assert(bmp_info->bit_count == 24 || bmp_info->bit_count == 32, "Expected 24 or 32 BPP");

    uint32_t pixel_data_len = size - (pixel_data - data);
    ret->pixel_data = malloc(pixel_data_len);
    memcpy(ret->pixel_data, pixel_data, pixel_data_len);
    ret->size = size_make(
        bmp_info->width,
        bmp_info->height
    );
    ret->bit_count = bmp_info->bit_count;
    return ret;
}

void image_free(image_bmp_t* image) {
    free(image->pixel_data);
    free(image);
}

void image_render_to_layer(image_bmp_t* image, ca_layer* dest, Rect frame) {
	float scale_x = image->size.width / (float)frame.size.width;
	float scale_y = image->size.height / (float)frame.size.height;
	int bpp = image->bit_count;
	int bytes_per_pixel = bpp / 8;

	for (uint32_t draw_row = 0; draw_row < frame.size.height; draw_row++) {
		int bmp_y = (image->size.height - 1) - (draw_row * scale_y);
		for (int32_t draw_col = 0; draw_col < frame.size.width; draw_col++) {
			int bmp_x = draw_col * scale_x;

			uint32_t bmp_off = (bmp_y * image->size.width * bytes_per_pixel) + (bmp_x * bytes_per_pixel);
			// Read as a u32 so we can get the whole pixel in one memory access
			uint32_t pixel = *((uint32_t*)(&image->pixel_data[bmp_off]));

			uint8_t r = (pixel >> 16) & 0xff;
			uint8_t g = (pixel >> 8) & 0xff;
			uint8_t b = (pixel >> 0) & 0xff;
			putpixel(dest, frame.origin.x + draw_col, frame.origin.y + draw_row, color_make(r, g, b));
		}
	}
}
