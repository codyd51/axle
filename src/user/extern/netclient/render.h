#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include "html.h"
#include "shims.h"
#include "layout.h"

typedef enum draw_command_enum {
    DRAW_COMMAND_RECTANGLE,
    DRAW_COMMAND_TEXT,
} draw_command_enum_t;

typedef struct draw_command_base {
    // Shared union members
    draw_command_enum_t cmd;
    Rect rect;
} draw_command_base_t;

typedef struct draw_command_rectangle {
    // Shared union members
    draw_command_enum_t cmd;
    Rect rect;

    // Public members
    Color color;
    uint32_t thickness;
} draw_command_rectangle_t;

typedef struct draw_command_text {
    // Shared union members
    draw_command_enum_t cmd;
    Rect rect;

    // Public members
    char* text;
    Color font_color;
    Size font_size;
} draw_command_text_t;

typedef union draw_command {
    draw_command_base_t base;
    draw_command_rectangle_t rect;
    draw_command_text_t text;
} draw_command_t;

array_t* draw_commands_generate_from_layout(layout_root_node_t* root_layout);

#endif