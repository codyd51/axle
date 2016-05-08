#include "gfx.h"
#include "std.h"

void int32_test() {
	int y;
	regs16_t regs;

	//switch to 320x200x256 gfx mode
	regs.ax = 0x0013;
	int32(0x10, &regs);

	//full screen with blue color
	memset((char*)0xA0000, 1, (320*200));

	//draw horizontal line from 100,80 to 100,240 in different colors
	for (y = 0; y < 200; y++) {
		memset((char*)0xA0000 + (y*320+80), y, 160);
	}

	//wait for key
	regs.ax = 0x0000;
	int32(0x16, &regs);

	//switch to 80x25x16 text mode
	regs.ax = 0x0003;
	int32(0x10, &regs);
}

