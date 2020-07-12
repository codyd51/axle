#include <std/std.h>
#include <std/math.h>
#include <std/timer.h>
#include <std/kheap.h>

#include <kernel/kernel.h>
#include <kernel/boot_info.h>
#include <kernel/vmm/vmm.h>
#include <kernel/multiboot.h>
#include <kernel/drivers/vga/vga.h>
#include <kernel/drivers/vbe/vbe.h>
#include <kernel/drivers/vesa/vesa.h>
#include <kernel/drivers/mouse/mouse.h>

#include <gfx/font/font.h>
#include <tests/gfx_test.h>

#include "gfx.h"
#include "bmp.h"
#include "color.h"
#include "shapes.h"


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

Vec2d vec2d(double x, float y) {
    Vec2d vec;
    vec.x = x;
    vec.y = y;
    return vec;
}

void gfx_teardown(Screen* screen) {
    if (!screen) return;

    //free screen
    window_teardown(screen->window);
    kfree(screen->vmem);
    kfree(screen);
}

void vsync() {
    //wait until previous retrace has ended
    do {} while (inb(0x3DA) & 8);

    //wait until new trace has just begun
    do {} while (!(inb(0x3DA) & 8));
}

void fill_screen(Screen* screen, Color color) {
    for (int y = 0; y < screen->resolution.height; y++) {
        for (int x = 0; x < screen->resolution.width; x++) {
            putpixel(screen->vmem, x, y, color);
        }
    }
    if (screen->vmem) {
        write_screen(screen);
    }
}

void write_screen(Screen* screen) {
    vsync();
    uint8_t* raw_double_buf = screen->vmem->raw;
    memcpy(screen->physbase, screen->vmem->raw, screen->resolution.width * screen->resolution.height * 3);
}

void write_screen_region(Rect region) {
    // TODO(PT): This function seems to cause page faults... ensure it writes in-bounds
    vsync();
    Screen* screen = gfx_screen();

    //bind input region to screen size
    region = rect_intersect(region, screen->window->frame);

    uint8_t* raw_double_buf = screen->vmem->raw;
    uint8_t* vmem = (uint8_t*)screen->physbase;

    int idx = (rect_min_y(region) * screen->resolution.width * screen->bytes_per_pixel) + (rect_min_x(region) * screen->bytes_per_pixel);
    for (int y = 0; y < region.size.height; y++) {
        //copy current row
        memcpy(vmem + idx, raw_double_buf + idx, region.size.width * screen->bytes_per_pixel);
        //advance to next row of region
        idx += screen->resolution.width * screen->bytes_per_pixel;
    }
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

    _screen.physbase = (uint32_t*)framebuffer_info.address;
    _screen.video_memory_size = framebuffer_info.size;

    _screen.resolution = size_make(framebuffer_info.width, framebuffer_info.height);
    _screen.bits_per_pixel = framebuffer_info.bits_per_pixel;
    _screen.bytes_per_pixel = framebuffer_info.bytes_per_pixel;

    // Font size is calculated as a fraction of screen size
    _screen.default_font_size = font_size_for_resolution(_screen.resolution);

    _screen.vmem = create_layer(_screen.resolution);
    _screen.window = create_window_int(rect_make(point_make(0, 0), _screen.resolution), true);
    _screen.window->superview = NULL;
    _screen.surfaces = array_m_create(128);

    _gfx_is_active = true;

    printf_info("Graphics: %d x %d, %d BPP", _screen.resolution.width, _screen.resolution.height, _screen.bits_per_pixel);
}

static Point cursor_pos = {0, 0};
void gfx_terminal_putchar(char c) {
    Screen* screen = gfx_screen();

    Size font_size = screen->default_font_size;
    Point new_cursor_pos = cursor_pos;

    int pad = 4;
    new_cursor_pos.x += font_size.width + pad;

    if (c == '\n' || new_cursor_pos.x + font_size.width + pad >= screen->resolution.width) {
        new_cursor_pos.y += font_size.height + pad;
        new_cursor_pos.x = 0;
    }
    if (new_cursor_pos.y + font_size.height >= screen->resolution.height) {
        // Reset cursor to origin and try again
        cursor_pos.x = cursor_pos.y = 0;
        gfx_terminal_putchar(c);
        return;
    }

    //draw_char(screen->vmem, c, new_cursor_pos.x, new_cursor_pos.y, printf_draw_color, font_size);
    draw_char(screen->vmem, c, new_cursor_pos.x, new_cursor_pos.y, color_black(), font_size);
    write_screen_region(rect_make(cursor_pos, font_size));

    cursor_pos = new_cursor_pos;
}

void gfx_terminal_puts(const char* str) {
    for (size_t i = 0; i < strlen(str); i++) {
        gfx_terminal_putchar(str[i]);
    }
}

void gfx_terminal_clear() {
    // Clear the screen's double buffer and redraw the background
    Deprecated();
}
