#include "bmp.h"
#include <std/std.h>
#include <kernel/util/vfs/fs.h>
#include "gfx.h"
#include <std/math.h>

void bmp_teardown(Bmp* bmp) {
	if (!bmp) return;

	layer_teardown(bmp->layer);
	kfree(bmp);
}

Bmp* create_bmp(Rect frame, ca_layer* layer) {
	if (!layer) return NULL;

	Bmp* bmp = (Bmp*)kmalloc(sizeof(Bmp));
	bmp->frame = frame;
	bmp->layer = layer;
	bmp->needs_redraw = 1;
	return bmp;
}

Bmp* _load_jpg(Rect frame, FILE* file) {
	NotImplemented();
	return NULL;
}

Bmp* _load_bmp(Rect frame, FILE* file) {
	unsigned char header[54];
	fread(&header, sizeof(char), 54, file);

	//get width and height from header
	int file_width = *(int*)&header[18];
	int file_height = *(int*)&header[22];
	int width = frame.size.width;
	int height = frame.size.height;

	printk_info("loading BMP with dimensions (%d,%d) scaled to (%d,%d)", file_width, file_height, width, height);

	//find scale factor of actual image dimensions to size requested
	float scale_x = width / (float)file_width;
	float scale_y = height / (float)file_height;

	int bpp = gfx_bytes_per_pixel();
	ca_layer* layer = create_layer(size_make(width, height));
	//image is upside down in memory so build array from bottom up
	//for (int draw_y = height - 1; draw_y >= 0; draw_y--) {
	for (int draw_y = 0; draw_y < height; draw_y++) {
		int translated_y = draw_y / scale_y;
		//for (int draw_x = width - 1; draw_x >= 0; draw_x--) {
		for (int draw_x = 0; draw_x < width; draw_x++) {
			int translated_x = draw_x / scale_x;

			int idx = (translated_y * file_width * bpp) + (translated_x * bpp);
			//if (idx + 2 > file_width * file_height * bpp) break;
			fseek(file, idx, SEEK_END);

			int draw_idx = (draw_y * width * bpp) + (draw_x * bpp);
			//we process 3 bytes at a time because image is stored in BGR, we need RGB
			layer->raw[draw_idx + 2] = fgetc(file);
			layer->raw[draw_idx + 1] = fgetc(file);
			layer->raw[draw_idx + 0] = fgetc(file);
		}
	}

	Bmp* bmp = create_bmp(frame, layer);
	printk_dbg("load_bmp() made bmp %x", bmp);

	return bmp;
}

Bmp* load_bmp(Rect frame, char* filename) {
	FILE* file = fopen(filename, (char*)"");
	if (!file) {
		printk_err("File %s not found! Not loading BMP", filename);
		return NULL;
	}

	Bmp* ret = NULL;
	//TODO check extension properly!
	if (strstr(filename, ".jpg") || strstr(filename, ".jpeg")) {
		ret = _load_jpg(frame, file);
	}
	else {
		ret = _load_bmp(frame, file);
	}

	fclose(file);
	return ret;
}


void draw_bmp(ca_layer* dest, Bmp* bmp) {
	if (!bmp) return;

	blit_layer(dest, bmp->layer, bmp->frame, rect_make(point_zero(), bmp->frame.size)); 

	bmp->needs_redraw = 0;
}

