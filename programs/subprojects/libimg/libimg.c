#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <memory.h>

#include <libutils/assert.h>
#include <agx/lib/putpixel.h>

#include "libimg.h"
#include "nanojpeg.h"

#define BMP_MAGIC 0x4D42	// 'BM' with flipped endianness
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

image_type_t _detect_type(uint8_t* data) {
	uint16_t* magic_cast = (uint16_t*)data;
	printf("Magic: 0x%04x\n", magic_cast[0]);
	if (magic_cast[0] == BMP_MAGIC) {
		return IMAGE_BITMAP;
	}
	return IMAGE_JPEG;
}

image_t* image_parse_bmp(uint32_t size, uint8_t* data) {
	bmp_file_header_t* bmp_header = (bmp_file_header_t*)data;
	bmp_info_header_t* bmp_info = (bmp_info_header_t*)(data + sizeof(bmp_file_header_t));

	uint8_t* pixel_data = (uint8_t*)(data + (bmp_header->data_offset));
	if (bmp_info->bit_count != 24 && bmp_info->bit_count != 32) {
		printf("bit count: %d\n", bmp_info->bit_count);
	}
	//assert(bmp_info->bit_count == 24 || bmp_info->bit_count == 32, "Expected 24 or 32 BPP");
	if (bmp_info->bit_count != 24 && bmp_info->bit_count != 32) {
		return NULL;
	}

    uint32_t pixel_data_len = size - (pixel_data - data);

    image_t* ret = calloc(1, sizeof(image_t));
	ret->type = IMAGE_BITMAP;
    ret->pixel_data = malloc(pixel_data_len);
    memcpy(ret->pixel_data, pixel_data, pixel_data_len);
    ret->size = size_make(
        bmp_info->width,
        bmp_info->height
    );
    ret->bit_count = bmp_info->bit_count;
    return ret;
}

image_t* image_parse_jpeg(uint32_t size, uint8_t* data) {
	static bool _has_done_nanojpeg_init = false;
	if (!_has_done_nanojpeg_init) {
		njInit();
		_has_done_nanojpeg_init = true;
	}

	nj_result_t result = njDecode(data, size);
	assert(result == NJ_OK, "Error decoding JPEG\n");
	assert(njIsColor(), "Not 24bpp color");

    image_t* ret = calloc(1, sizeof(image_t));
	ret->type = IMAGE_JPEG;
	ret->bit_count = 24;
	uint8_t bytes_per_pixel = ret->bit_count / 8;
	ret->size = size_make(njGetWidth(), njGetHeight());

	uint32_t pixel_data_size = njGetImageSize();
	uint8_t* pixel_data = calloc(1, pixel_data_size);
	uint8_t* orig_pixel_data = njGetImage();
	for (int32_t row = 0; row < ret->size.height; row++) {
		int32_t flipped_y = (ret->size.height - 1) - (row);
		for (uint32_t col = 0; col < ret->size.width; col++) {
			uint32_t offset = (row * ret->size.width * bytes_per_pixel) + (col * bytes_per_pixel);
			uint32_t flipped_offset = (flipped_y * ret->size.width * bytes_per_pixel) + (col * bytes_per_pixel);
			// Read as a u32 so we can get the whole pixel in one memory access
			uint32_t pixel = *((uint32_t*)(&orig_pixel_data[offset]));

			uint8_t r = (pixel >> 16) & 0xff;
			uint8_t g = (pixel >> 8) & 0xff;
			uint8_t b = (pixel >> 0) & 0xff;

			uint32_t flipped_pixel = (b << 16) | (g << 8) | (r);

			uint32_t* ptr = (uint32_t*)(&pixel_data[flipped_offset]);
			*ptr = flipped_pixel;
		}
	}

	ret->pixel_data = pixel_data;

	njDone();

    return ret;
}

image_t* image_parse(uint32_t size, uint8_t* data) {
	image_type_t type = _detect_type(data);
	if (type == IMAGE_BITMAP) {
		return image_parse_bmp(size, data);
	}
	return image_parse_jpeg(size, data);
}

void image_free(image_t* image) {
    free(image->pixel_data);
    free(image);
}

void image_render_to_layer(image_t* image, ca_layer* dest, Rect frame) {
	// TODO(PT): Update image rendering to update a gui_scroll_layer's max_y
	float scale_x = 1.0;
	float scale_y = 1.0;
	if (frame.size.width != image->size.width || frame.size.height != image->size.height) {
		//printf("Scaling image %d %d to %d %d\n", image->size.width, image->size.height, frame.size.width, frame.size.height);
		scale_x = image->size.width / (float)frame.size.width;
		scale_y = image->size.height / (float)frame.size.height;
	}
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
