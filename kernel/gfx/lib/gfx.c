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
