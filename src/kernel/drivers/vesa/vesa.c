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
Window* create_window_int(Rect frame, bool root);

//sets up VESA for mode
Screen* switch_to_vesa(uint32_t vesa_mode, bool create) {
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
	//TODO figure out why this isn't a pointer
	//things break if we make this a pointer
	//but it's a leak ATM
	uint32_t mode_buffer = (uint32_t)(kmalloc(sizeof(vbe_mode_info))) & 0xFFFFF;

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
	memcpy(&mode_info, (uint32_t*)mode_buffer, sizeof(vbe_mode_info));

	regs.ax = 0x4F02; //02 sets graphics mode

	//sets up mode with linear frame buffer instead of bank switching
	//or 0x4000 turns on linear frame buffer
	regs.bx = (vesa_mode | 0x4000);
	int32(0x10, &regs);
		
	//screen_create depends on knowing gfx_bpp, so we must call this with NULL for the screen to create the screen,
	//and then we can call it normally after the screen is created
	process_gfx_switch(NULL, mode_info.bpp);

	kernel_end_critical();

	if (create) {
		printk("screen_create (%d, %d), physbase 0x%x, %d bpp\n", mode_info.x_res, mode_info.y_res, mode_info.physbase, mode_info.bpp);
		Screen* screen = screen_create(size_make(mode_info.x_res, mode_info.y_res), (uint32_t*)mode_info.physbase, mode_info.bpp);
		process_gfx_switch(screen, mode_info.bpp);
		return screen;
	}

	return 0;
}
