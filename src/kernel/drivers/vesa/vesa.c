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
	regs.ax = 0x4F02;
	regs.cx = 0x18113; //mode 112h => 640x480 24-bit
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

void putpixel_16rgb(unsigned char color, unsigned short x, unsigned short y, uint16_t pitch) {
	unsigned char* fb = (unsigned char*)0xa0000;
	unsigned int offset = x + (long)y * pitch;
	fb[offset] = color;
}

#define SEG(x) 

vesa_mode_info* switch_to_vesa() {
	vesa_mode_info* info = kmalloc(512);

	regs16_t regs;
	regs.ax = 0x4F02;
	regs.cx = 0x18112;

	printf_info("regs.es %x", regs.es);
	printf_info("regs.di %x", regs.di);

	sleep(3000);

//	int y = bufPos / VGA_WIDTH;
//	int x = bufPos - (y * VGA_WIDTH);
//	y * width + x
	//unsigned int addr = &info;
	//regs.es = addr / 16;
	//regs.di = addr - (regs.es);

	//regs.es = SEG(info);
	//regs.di = OFF(vib);
	
	unsigned int addr = &info;
	regs.di = addr & 0xF;
	regs.es = (addr >> 4) & 0xFFFF;

	int32(0x10, &regs);
	//vesa_mode_info* info = regs.es | (regs.di << 16);

	switch_to_text();

	printf_info("regs.es AF %x", regs.es);
	printf_info("regs.di AF %x", regs.di);

	printf_info("addr: %x", addr);

	return info;
}

void vesa_test() {
	/*
	vesa_mode_info* mode_info = get_vesa_info();
	//fill_screen_v(mode_info, 100, 100, 100);
	for (int x = 0; x < 640; x++) {
		for (int y = 0; y < 480; y++) {
			putpixel_16rgb(234, x, y, mode_info->pitch);
		}
	}

	//switch_to_text();
	printf_info("mode_info: %x", mode_info);
	printf_info("physbase: %x", mode_info->physbase);
	printf_info("x_res: %d", mode_info->x_res);
       	printf_info("y_res: %d", mode_info->y_res);
	*/

	vesa_mode_info* mode_info = switch_to_vesa();
	//switch_to_text();
	//printf_info("mode_info: %x", mode_info);
	//printf_info("x_res: %d", mode_info->x_res);
	//printf_info("y_res: %d", mode_info->y_res);
	//printf_info("physbase: %x", mode_info->physbase);

	//printf_info("out.ax: %x", regs.ax);
	//if (regs.ax != 0x004f) printf_info("something went wrong with vbe get info");
	//else printf_info("vbe get info successful");
	//printf_info("AFTER vib: %x", vib);
}

