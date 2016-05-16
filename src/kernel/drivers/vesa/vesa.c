#include "vesa.h"
#include <std/std.h>
#include <gfx/lib/gfx.h>
/*gfx
void init_vesa() {
	vbe_info_block* vib = malloc(512);

	regs16_t regs;
	regs.ax = 0x4f00;
	regs.es = SEG(vib);
	regs.di = OFF(vib);
	
	int32(0x10, &regs);

	if (regs.ax != 0x004f) printf("VBE get info failed.\n");
}

typedef struct vesa_screen_t {
	u16int granularity;
	u16int winsize;
	u16int pitch;

	u16int x_res, y_res;

	u8int red_mask, red_position;
	u8int green_mask, green_position;
	u8int blue_mask, blue_position;
}*/

vesa_mode_info* get_vesa_info() {
	regs16_t regs;
	regs.ax = 0x4F01;
	regs.cx = 0x0112; //mode 112h => 640x480 24-bit
	int32(0x10, &regs);
	vesa_mode_info* info = regs.es | (regs.di << 16);
	return info;
}

vesa_mode_info* init_vesa() {
	vesa_mode_info* info = get_vesa_info();
	//vesa_screen_t* screen = malloc(sizeof(vesa_screen_t));
	//screen->info = info;
	return info;
}

void putpixel_v(vesa_mode_info* mode_info, int x, int y, int color) {
	unsigned loc = mode_info->physbase + (y * mode_info->x_res) + x;
	memset(loc, color, 1);
}

void fill_screen_v(vesa_mode_info* mode_info, unsigned char r, unsigned char g, unsigned char b) {
	//memset((char*)mode_info->physbase, color, (mode_info->x_res * mode_info->y_res));
	unsigned char* where = mode_info->physbase;
	int i, j;

	for (int i = 0; i < mode_info->x_res; i++) {
		for (j = 0; j < mode_info->y_res; j++) {
			where[j * 4] = r;
			where[j * 4 + 1] = g;
			where[j * 4 + 2] = b;
		}
		where += 3200;
	}
}

void vesa_test() {
	vesa_mode_info* mode_info = get_vesa_info();
	fill_screen_v(mode_info, 100, 100, 100);
}

