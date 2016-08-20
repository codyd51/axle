#include "vesa.h"
#include <kernel/util/paging/paging.h>
#include <gfx/lib/shapes.h>
#include <gfx/lib/view.h>
#include <gfx/lib/gfx.h>
#include <user/xserv/xserv.h>
#include <std/memory.h>
#include <kernel/drivers/kb/kb.h>
#include <kernel/kernel.h>
#include <std/timer.h>
#include <kernel/drivers/rtc/clock.h>

extern page_directory_t* kernel_directory;

//sets bank if LFB isn't supported/enabled
void set_bank(int bank) {
		static int previous_bank = -1;
		//if we're already on the requested bank, quit early
		if (bank == previous_bank) return;

		regs16_t regs;

		regs.ax = 0x4F05;
		regs.bx = 0x0;
		regs.dx = bank;

		int32(0x10, &regs);

		previous_bank = bank;
}

//sets up VESA for mode
Screen* switch_to_vesa(uint32_t vesa_mode) {
		kernel_begin_critical();
		
		vesa_info info;
		vbe_mode_info mode_info;
		regs16_t regs;

		//get VESA information
		
		//buffer stores info before being copied into structure	
		uint32_t buffer = (uint32_t)kmalloc(sizeof(vesa_info)) & 0xFFFFF;

		memcpy((void*)buffer, "VBE2", 4);
		memset(&regs, 0, sizeof(regs));

		regs.ax = 0x4F00; //00 gets VESA information
		regs.di = buffer & 0xF;
		regs.es = (buffer >> 4) & 0xFFFF;
		int32(0x10, &regs);

		//copy info from buffer into struct
		memcpy(&info, (void*)buffer, sizeof(vesa_info));

		//get VESA mode information

		//buffer to store mode info before copying into structure
		uint32_t mode_buffer = (uint32_t)kmalloc(sizeof(vbe_mode_info)) & 0xFFFFF;

		memset(&regs, 0, sizeof(regs));

		//VESA mode
		//0x118: 1024x768x24
		//0x112: 640x400x24

		regs.ax = 0x4F01; //01 gets VBE mode information
		regs.di = mode_buffer & 0xF;
		regs.es = (mode_buffer >> 4) & 0xFFFF;
		regs.cx = vesa_mode; //mode to get info for
		int32(0x10, &regs);
		
		//copy mode info from buffer into struct
		memcpy(&mode_info, (void*)mode_buffer, sizeof(vbe_mode_info));
	
		regs.ax = 0x4F02; //02 sets graphics mode

		//sets up mode with linear frame buffer instead of bank switching
		//or 0x4000 turns on linear frame buffer
		regs.bx = (vesa_mode | 0x4000);
		int32(0x10, &regs);

		Screen* screen = (Screen*)kmalloc(sizeof(Screen));
		screen->vmem = (uint8_t*)kmalloc(mode_info.x_res * mode_info.y_res * (mode_info.bpp / 8));
		screen->depth = mode_info.bpp;
		//linear frame buffer (LFB) address
		screen->physbase = (uint8_t*)mode_info.physbase;
		screen->window = create_window(rect_make(point_make(0, 0), size_make(mode_info.x_res, mode_info.y_res)));
		
		kernel_end_critical();

		return screen;
}

void putpixel_vesa(Screen* screen, int x, int y, Color color) {
		int offset = x * (screen->depth / 8) + y * (screen->window->size.width * (screen->depth / 8));
		//we have to write the pixels in BGR, not RGB
		screen->vmem[offset + 0] = color.val[2];
		screen->vmem[offset + 1] = color.val[1];
		screen->vmem[offset + 2] = color.val[0];
}
