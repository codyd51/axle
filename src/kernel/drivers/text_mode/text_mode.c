#include "text_mode.h"
#include <std/ctype.h>

typedef uint16_t text_mode_entry;
static const size_t TEXT_MODE_WIDTH = 80;
static const size_t TEXT_MODE_HEIGHT = 25;

static text_mode_color text_mode_color_make(text_mode_color_component foreground, text_mode_color_component background) {
    return (text_mode_color)(foreground | (background << 4));
}

static text_mode_entry text_mode_entry_make(unsigned char ch, text_mode_color color) {
   return (uint16_t)ch | ((uint16_t)color << 8);
}

typedef struct screen_state {
    size_t cursor_row;
    size_t cursor_col;
    text_mode_color color;
    uint16_t* buffer;
} screen_state_t;
screen_state_t screen_state;

void text_mode_clear() {
    screen_state.cursor_row = 0;
    screen_state.cursor_col = 0;
    for (size_t y = 0; y < TEXT_MODE_HEIGHT; y++) {
        for (size_t x = 0; x < TEXT_MODE_WIDTH; x++) {
            const size_t index = y * TEXT_MODE_WIDTH + x;
            screen_state.buffer[index] = text_mode_entry_make(' ', screen_state.color);
        }
    }
}

static void text_mode_set_color(text_mode_color col) {
    screen_state.color = col;
}

void text_mode_init() {
    //TODO(PT): define the framebuffer location, or pull it from multiboot info
    screen_state.buffer = (uint16_t*)0xB8000;
    text_mode_set_color(text_mode_color_make(TEXT_MODE_COLOR_WHITE, TEXT_MODE_COLOR_BLACK));
    text_mode_clear();
}

static void text_mode_scroll_up_line(void) {
    for (size_t y = 1; y < TEXT_MODE_HEIGHT; y++) {
        for (size_t x = 0; x < TEXT_MODE_WIDTH; x++) {
            const size_t index = y * TEXT_MODE_WIDTH + x;
            //copy the data here to the spot 1 row above
            const size_t above_index = (y - 1) * TEXT_MODE_WIDTH + x;
            screen_state.buffer[above_index] = screen_state.buffer[index];
        }
    }
    //empty bottom line
    const bottom_row = TEXT_MODE_HEIGHT - 1;
    for (size_t x = 0; x < TEXT_MODE_WIDTH; x++) {
        const size_t index = bottom_row * TEXT_MODE_WIDTH + x;
        screen_state.buffer[index] = text_mode_entry_make(' ', screen_state.color);
    }
}

static void text_mode_newline(void) {
    screen_state.cursor_col = 0;
    if (screen_state.cursor_row == TEXT_MODE_HEIGHT - 1) {
        text_mode_scroll_up_line();
    }
    else {
        screen_state.cursor_row++;
    }
}

static void text_mode_tab(void) {
    const int tab_len = 4;
    for (int i = 0; i < tab_len; i++) {
        text_mode_putchar(' ');
    }
}

void text_mode_place_char(unsigned char ch, text_mode_color color, size_t x, size_t y) {
    const size_t index = y * TEXT_MODE_WIDTH + x;
    screen_state.buffer[index] = text_mode_entry_make(ch, screen_state.color);
}

static void text_mode_cursor_increment(void) {
    screen_state.cursor_col++;
    if (screen_state.cursor_col >= TEXT_MODE_WIDTH) {
        text_mode_newline();
    }
}

static void text_mode_putchar_printable(unsigned char ch) {
    text_mode_place_char(ch, screen_state.color, screen_state.cursor_col, screen_state.cursor_row);
    text_mode_cursor_increment();
}

static void text_mode_putchar_special(unsigned char ch) {
    // TODO(PT): verify ch is a special char!
    switch (ch) {
        case '\n':
            text_mode_newline();
            break;
        case '\t':
            text_mode_tab();
            break;
        default:
            break;
    }
}

void text_mode_putchar(unsigned char ch) {
    if (isprint(ch)) {
        text_mode_putchar_printable(ch);
    }
    else {
        text_mode_putchar_special(ch);
    }
}

void text_mode_write(const char* str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        // TODO(PT) add check for hitting null before len
        text_mode_putchar(str[i]);
    }
}

void text_mode_puts(const char* str) {
    text_mode_write(str, strlen(str));
}
