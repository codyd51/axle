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

Bmp* load_bmp(Rect frame, char* filename) {
	//return NULL;
	FILE* file = fopen(filename, (char*)"");
	if (!file) {
		printk_err("File %s not found! Not loading BMP", filename);
		return NULL;
	}

	unsigned char header[54];
	fread(&header, sizeof(char), 54, file);

	//get width and height from header
	int file_width, file_height;
	int width = file_width = *(int*)&header[18];
	int height = file_height = *(int*)&header[22];

	//don't exceed given frame
	width = MIN(width, frame.size.width);
	height = MIN(height, frame.size.height);
	printk_info("loading BMP %s with dimensions (%d,%d)", filename, width, height);

	int bpp = gfx_bpp();
	ca_layer* layer = create_layer(size_make(width, height));
	printk_dbg("load_bmp() got layer %x", layer);
	//image is upside down in memory so build array from bottom up
	for (int y = file_height - 1; y >= 0; y--) {
		/*
		if (y >= height) {
			//y is too large to fit in layer
			//eat bytes of this pixel
			fgetc(file);
			fgetc(file);
			fgetc(file);
			continue;
		}
		*/
		for (int x = file_width - 1; x >= 0; x--) {
			/*
			if (x >= width) {
				//x is too large to fit in layer
				//eat bytes of this pixel
				fgetc(file);
				fgetc(file);
				fgetc(file);
				continue;
			}
			*/

			//if (y == file_height && x == file_width) continue;

			int idx = (y * width * bpp) + (x * bpp);
			if (idx > width * height * bpp) break;

			//we process 3 bytes at a time because image is stored in BGR, we need RGB
			layer->raw[idx + 0] = fgetc(file);
			layer->raw[idx + 1] = fgetc(file);
			layer->raw[idx + 2] = fgetc(file);
		}
	}

	Bmp* bmp = create_bmp(frame, layer);
	printk_dbg("load_bmp() made bmp %x", bmp);

	fclose(file);
	return bmp;
}
