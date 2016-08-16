#include "vesa.h"
#include <kernel/util/paging/paging.h>
#include <gfx/lib/shapes.h>
#include <gfx/lib/view.h>
#include <gfx/lib/gfx.h>
#include <user/xserv/xserv.h>
#include <std/memory.h>
#include <kernel/drivers/kb/kb.h>
#include <kernel/kernel.h>

static Label* fps;
void vesa_screen_refresh(Screen* screen) {
	//check if there are any keys pending
	while (haskey()) {
		char ch = getchar();
		if (ch == 'q') {
			//quit xserv
			gfx_teardown(screen);
			switch_to_text();
			return;
		}
	}

	if (!screen->finished_drawing) return;

	//if no changes occured this refresh, don't bother writing the screen
	/*
	if (xserv_draw(screen)) {
		write_screen(screen);
	}
	*/

	static double timestamp = 0; //current frame timestamp
	static double time_prev = 0; //prev frame timestamp
	
	xserv_draw(screen);

	time_prev = timestamp;
	timestamp = time();
	double frame_time = (timestamp - time_prev) / 1000.0;
	//update frame time tracker 
	char buf[32];
	itoa(frame_time * 100000, &buf);
	strcat(buf, " ns/frame");
	fps->text = buf;
	draw_label(screen, fps);

	write_screen(screen);
}

void setup_vesa_screen_refresh(Screen* screen, double interval) {
	screen->callback = add_callback((void*)vesa_screen_refresh, interval, true, screen);
}

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
		set_frame(screen->window->title_view, rect_make(point_make(0, 0), size_make(0, 0)));
		set_frame(screen->window->content_view, screen->window->frame);
		set_border_width(screen->window, 0);
		desktop_setup(screen);

		//add FPS tracker
		fps = create_label(rect_make(point_make(3, 3), size_make(300, 50)), "FPS counter");
		fps->text_color = color_black();
		add_sublabel(screen->window->content_view, fps);

		//start refresh loop
		setup_vesa_screen_refresh(screen, 100);
		//refresh once now so we don't wait for the first tick
		//vesa_screen_refresh(screen);

		kernel_end_critical();

		return screen;
}

void putpixel_vesa(Screen* screen, int x, int y, Color color) {
		int offset = x * (screen->depth / 8) + y * (screen->window->size.width * (screen->depth / 8));
		uint32_t hex = color_hex(color);
		screen->vmem[offset + 0] = hex & 0xFF;
		screen->vmem[offset + 1] = (hex >> 8) & 0xFF;
		screen->vmem[offset + 2] = (hex >> 16) & 0xFF;
}
