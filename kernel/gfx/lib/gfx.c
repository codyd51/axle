#include <std/std.h>
#include <std/math.h>
#include <std/timer.h>
#include <std/kheap.h>

#include <kernel/kernel.h>
#include <kernel/boot_info.h>
#include <kernel/vmm/vmm.h>
#include <kernel/multiboot.h>
#include <kernel/drivers/mouse/mouse.h>

#include <gfx/font/font.h>

#include "gfx.h"
#include "color.h"
#include "screen.h"


//private Window function to create root window
Window* create_window_int(Rect frame, bool root);
void gfx_terminal_clear();

static int current_depth = 0;
static Screen _screen = {0};
static bool _gfx_is_active = false;

inline int gfx_depth() {
    Deprecated();
    return -1;
}

inline int gfx_bpp() {
    Deprecated();
    return -1;
}

inline int gfx_bytes_per_pixel() {
    if (!_screen.bytes_per_pixel) panic("may not be called before gfx stack is active");
    return _screen.bytes_per_pixel;
}

inline int gfx_bits_per_pixel() {
    if (!_screen.bits_per_pixel) panic("may not be called before gfx stack is active");
    return _screen.bits_per_pixel;
}

Screen* gfx_screen() {
    if (!_gfx_is_active) return NULL;
    return &_screen;
}

void gfx_teardown(void) {
    Deprecated();
}

void vsync() {
    //wait until previous retrace has ended
    do {} while (inb(0x3DA) & 8);

    //wait until new trace has just begun
    do {} while (!(inb(0x3DA) & 8));
}

Size font_size_for_resolution(Size resolution) {
    Size size = {12, 12};
    const int required_rows = 60;
    const int required_cols = 60;
    //shrink font size until we can at least fit 80 chars on a line * 20 lines
    //if we can't fit more than 20 characters on a line, shrink font and try again
    while (resolution.width / size.width < required_rows) {
        size.width /= 1.5;
    }
    while (resolution.height / size.height < required_cols) {
        size.height /= 1.5;
    }
    return size;
}

Screen* gfx_init(void) {
    framebuffer_info_t framebuffer_info = boot_info_get()->framebuffer; 

    _screen.physbase = (uintptr_t*)framebuffer_info.address;
    _screen.video_memory_size = framebuffer_info.size;

    _screen.resolution = size_make(framebuffer_info.width, framebuffer_info.height);
    _screen.bits_per_pixel = framebuffer_info.bits_per_pixel;
    _screen.bytes_per_pixel = framebuffer_info.bytes_per_pixel;

    // Font size is calculated as a fraction of screen size
    //_screen.default_font_size = font_size_for_resolution(_screen.resolution);

    //_screen.vmem = create_layer(_screen.resolution);
    //_screen.window = create_window_int(rect_make(point_make(0, 0), _screen.resolution), true);
    //_screen.window->superview = NULL;
    //_screen.surfaces = array_m_create(128);

    _gfx_is_active = true;

    //printf_info("Graphics: %d x %d, %d BPP", _screen.resolution.width, _screen.resolution.height, _screen.bits_per_pixel);
}

void gfx_terminal_clear() {
    // Clear the screen's double buffer and redraw the background
    Deprecated();
}

void kernel_gfx_fill_screen(Color color) {
    boot_info_t* b = boot_info_get();
    framebuffer_info_t fb = b->framebuffer;
    uintptr_t addr = vmm_is_active() ? PMA_TO_VMA(fb.address) : fb.address;
    uint32_t* base = (uint32_t*)addr;
    uint32_t color_as_u32 = color.val[0] << 16 | color.val[1] << 8 | color.val[2];
	for (uint32_t y = 0; y < fb.height; y++) {
		for (uint32_t x = 0; x < fb.width; x++) {
			base[(y * fb.width) + x] = color_as_u32;
		}
	}
}

void kernel_gfx_fill_rect(Rect r, Color color) {
    boot_info_t* b = boot_info_get();
    framebuffer_info_t fb = b->framebuffer;

    // Bind the rect to the screen size
    r.origin.x = max(0, r.origin.x);
    r.origin.y = max(0, r.origin.y);
    r.size.width = min(fb.width, r.size.width);
    r.size.height = min(fb.height, r.size.height);

    // TODO(PT): This only works with 4BPP
    uintptr_t addr = vmm_is_active() ? PMA_TO_VMA(fb.address) : fb.address;
    uint32_t* base = (uint32_t*)addr;

    uint32_t color_as_u32 = color.val[0] << 16 | color.val[1] << 8 | color.val[2];
	for (uint32_t y = rect_min_y(r); y < rect_max_y(r); y++) {
		for (uint32_t x = rect_min_x(r); x < rect_max_x(r); x++) {
			base[(y * fb.width) + x] = color_as_u32;
		}
	}
}

Size kernel_gfx_screen_size(void) {
    boot_info_t* b = boot_info_get();
    framebuffer_info_t fb = b->framebuffer;
    return size_make(fb.width, fb.height);
}

static Point _g_cursor = {0, 0};
static Size _g_font_size = {8, 12};

void kernel_gfx_putpixel(uint8_t* dest, int x, int y, Color color) {
    boot_info_t* b = boot_info_get();
    framebuffer_info_t fb = b->framebuffer;
    int width = fb.width;
    int height = fb.height;
    int bpp = fb.bytes_per_pixel;
	//don't attempt writing a pixel outside of screen bounds
	if (x < 0 || y < 0 || x >= width || y >= height) return;

	uint32_t offset = (x * bpp) + (y * width * bpp);
	for (uint32_t i = 0; i < 3; i++) {
		// Pixels are written in BGR, not RGB
		// Thus, flip color order when reading a source color-byte
		dest[offset + i] = color.val[bpp - i - 1];
	}
}

#include <gfx/font/font8x8.h>

Point kernel_gfx_draw_string(uint8_t* dest, char* str, Point origin, Color color, Size font_size) {
	int x = origin.x;
	int y = origin.y;
	int string_len = strlen(str);
	int idx = 0;
    Size padding = size_make(0, 0);
    uint32_t height = boot_info_get()->framebuffer.height;
	while (str[idx]) {
		if (str[idx] == '\n') {
			x = 0;

			//quit if going to next line would exceed view bounds
			if ((y + font_size.height + padding.height + 1) >= height) break;
			y += font_size.height + padding.height;
			idx++;
			continue;
		}

		Color draw_color = color;
		kernel_gfx_draw_char(dest, str[idx], x, y, draw_color, font_size);

		x += font_size.width + padding.width;
		idx++;
	}
    return point_make(x, y);
}

void kernel_gfx_set_line_rendered_string_cursor(Point new_cursor_pos) {
    _g_cursor = new_cursor_pos;
}

void kernel_gfx_write_line_rendered_string_ex(char* str, bool higher_half) {
    boot_info_t* b = boot_info_get();
    framebuffer_info_t fb = b->framebuffer;
    uint64_t addr = fb.address;
    if (higher_half) {
        addr = PMA_TO_VMA(addr);
    }

    Point end_cursor = kernel_gfx_draw_string((uint8_t*)addr, str, _g_cursor, color_white(), _g_font_size);
    _g_cursor.x = 0;
    _g_cursor.y = end_cursor.y + (_g_font_size.height * 2);
}

void kernel_gfx_write_line_rendered_string(char* str) {
    kernel_gfx_write_line_rendered_string_ex(str, true);
}
