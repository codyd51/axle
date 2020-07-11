#include <std/std.h>
#include <std/math.h>
#include <std/timer.h>
#include <std/kheap.h>

#include <kernel/kernel.h>
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
static Screen* current_screen = 0;
void process_gfx_switch(Screen* screen, int new_depth) {
    current_screen = screen;
    current_depth = new_depth;
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
    return current_screen;
}

Vec2d vec2d(double x, float y) {
    Vec2d vec;
    vec.x = x;
    vec.y = y;
    return vec;
}

Screen* screen_create(Size dimensions, uint32_t* physbase, uint8_t depth) {
    Screen* screen = kmalloc(sizeof(Screen));

    //linear frame buffer (LFB) address
    screen->physbase = physbase;
    screen->window = create_window_int(rect_make(point_make(0, 0), dimensions), true);
    screen->window->superview = NULL;
    screen->depth = depth;
    //8 bits in a byte
    screen->bpp = depth / 8;
    screen->vmem = create_layer(dimensions);
    screen->resolution = dimensions;

    screen->surfaces = array_m_create(128);
    printk_info("screen surfaces 0x%x", screen->surfaces);
    printk_info("screen surfaces size 0x%x", screen->surfaces->size);

    return screen;
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
    uint8_t* raw_vmem = (uint8_t*)VBE_DISPI_LFB_PHYSICAL_ADDRESS;
    uint8_t* raw_double_buf = screen->vmem->raw;

    //video memory uses bank switching
    //figure out how many banks we'll need to write to
    int bytes_on_screen = (screen->resolution.width * screen->resolution.height * gfx_bpp());
    int banks_needed = bytes_on_screen / BANK_SIZE;
    for (int bank = 0; bank <= banks_needed; bank++) {
        vbe_set_bank(bank);
        memcpy(raw_vmem, raw_double_buf + (BANK_SIZE * bank), BANK_SIZE);
    }
}

void write_screen_region(Rect region) {
    Screen* screen = gfx_screen();

    //bind input region to screen size
    region = rect_intersect(region, screen->window->frame);

    //vsync();
    uint8_t* raw_vmem = (uint8_t*)VBE_DISPI_LFB_PHYSICAL_ADDRESS;
    uint8_t* raw_double_buf = screen->vmem->raw;
    int idx = (rect_min_y(region) * screen->resolution.width * screen->bpp) + (rect_min_x(region) * screen->bpp);

    for (int y = 0; y < region.size.height; y++) {
        int bank = idx / BANK_SIZE;
        vbe_set_bank(bank);
    int offset = idx % BANK_SIZE;
        //copy current row
        //dest: bank window + offset from bank start
        //src: vmem + real idx of screen vmem
        memcpy(raw_vmem + offset, raw_double_buf + idx, region.size.width * screen->bpp);
        //advance to next row of region
        idx += screen->resolution.width * screen->bpp;
    }
}

void rainbow_animation(Screen* screen, Rect r, int animationStep) {
    //ROY G BIV
    //int colors[] = {4, 42, 44, 46, 1, 13, 34};
    Color colors[] = {color_red(),
                      color_orange(),
                      color_yellow(),
                      color_green(),
                      color_blue(),
                      color_purple(),
    };
    int count = sizeof(colors) / sizeof(colors[0]);

    for (int i = 0; i < count; i++) {
        Point origin = point_make(r.origin.x + (r.size.width / count) * i, r.origin.y);
        Size size = size_make((r.size.width / count), r.size.height);
        Rect seg = rect_make(origin, size);

        Color col = colors[i];
        draw_rect(screen->vmem, seg, col, THICKNESS_FILLED);
        write_screen_region(seg);
        
        sleep(animationStep / count);
    }
}

void display_boot_screen() {
    Screen* screen = gfx_screen();
    fill_screen(screen, color_black());

    //TODO: Draw new logo
    Point p1 = point_make(screen->resolution.width / 2, screen->resolution.height * 0.25);
    Point p2 = point_make(screen->resolution.width / 2 - screen->resolution.width / 10, screen->resolution.height * 0.5);
    Point p3 = point_make(screen->resolution.width / 2 + screen->resolution.width / 10, screen->resolution.height * 0.5);
    Triangle triangle = triangle_make(p1, p2, p3);
    draw_triangle(screen->vmem, triangle, color_green(), THICKNESS_FILLED);

    Size default_size = screen->default_font_size;
    Size font_size = size_make(default_size.width * 2, default_size.height * 2);
    char* label_text = "axle os";
    int text_width = strlen(label_text) * font_size.width;
    Point lab_origin = point_make((screen->resolution.width / 2) - (text_width / 2), screen->resolution.height * 0.6);
    draw_string(screen->vmem, label_text, lab_origin, color_white(), font_size);

    float rect_length = screen->resolution.width / 3;
    Point origin = point_make((screen->resolution.width / 2) - (rect_length / 2), (screen->resolution.height / 4) * 3);
    Size sz = size_make(rect_length - 5, screen->resolution.height / 16);
    Rect border_rect = rect_make(origin, sz);

    //fill the rectangle with white initially
    draw_rect(screen->vmem, border_rect, color_white(), THICKNESS_FILLED);
    
    write_screen(screen);

    sleep(500);

    Point rainbow_origin = point_make(origin.x + 2, origin.y + 2);
    Size rainbow_size = size_make(rect_length - 6, sz.height - 3);
    Rect rainbow_rect = rect_make(rainbow_origin, rainbow_size);
    rainbow_animation(screen, rainbow_rect, 1000);

    sleep(500);
    fill_screen(screen, color_black());
}

static Size font_size_for_resolution(Size resolution) {
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

void gfx_init(struct multiboot_info* mboot_ptr) {
    struct multiboot_info* mboot = (struct multiboot_info*)mboot_ptr;
    vbe_mode_info* mode = (vbe_mode_info*)mboot->vbe_mode_info;
    static Screen screen;

    screen.resolution = size_make(mode->x_res, mode->y_res);
    screen.vmem = create_layer(screen.resolution);
    screen.depth = mode->bpp;
    screen.bpp = screen.depth / 8;
    screen.window = NULL;
    screen.surfaces = array_m_create(128);
    process_gfx_switch(&screen, mode->bpp);

    //set default font size to fraction of screen size
    Size s = font_size_for_resolution(screen.resolution);
    screen.default_font_size = s;

    Size padding = font_padding_for_size(s);
    printf_info("Running in %d x %d x %d", screen.resolution.width, screen.resolution.height, screen.depth);
    printf_info("Recommended font size is %dx%d, recommended padding is %dx%d", s.width, s.height, padding.width, padding.height);
}

static Point cursor_pos = {0, 0};
void gfx_terminal_putchar(char c) {
    Screen* screen = gfx_screen();
    Size font_size = screen->default_font_size;

    //Point gfx_get_cursor_pos();
    //Point old_cursor_pos = gfx_get_cursor_pos();
    //Point new_cursor_pos = old_cursor_pos;
    Point new_cursor_pos = cursor_pos;

    int pad = 3;
    new_cursor_pos.x += font_size.width + pad;

    if (c == '\n' || new_cursor_pos.x + font_size.width + pad >= screen->resolution.width) {
        new_cursor_pos.y += font_size.height + pad;
        new_cursor_pos.x = 0;
    }
    if (new_cursor_pos.y + font_size.height >= screen->resolution.height) {
        gfx_terminal_clear();
    }
    //else {
        //if (c != '\n') {
            //draw_char(screen->vmem, c, new_cursor_pos.x, new_cursor_pos.y, printf_draw_color, font_size);
            draw_char(screen->vmem, c, new_cursor_pos.x, new_cursor_pos.y, color_white(), font_size);
            write_screen_region(rect_make(cursor_pos, font_size));
        //}
    //}
    cursor_pos = new_cursor_pos;

}

void gfx_terminal_clear() {
    //clear screen, redraw background
    fill_screen(gfx_screen(), color_black());
    //gfx_set_cursor_pos(0, 0);
    cursor_pos.x = cursor_pos.y = 0;
    write_screen(gfx_screen());
}
