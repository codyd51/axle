#ifndef GUI_LAYER_H
#define GUI_LAYER_H

#include <libagx/lib/ca_layer.h>
// For ca_scrolling_layer, can remove once refactored
#include <libagx/lib/text_box.h>

#include <libagx/lib/shapes.h>
#include "gui_elem.h"

typedef enum gui_layer_type {
    GUI_FIXED_LAYER = 0,
    GUI_SCROLL_LAYER = 1
} gui_layer_type_t;

typedef struct gui_base_layer {
    gui_layer_type_t type;
} gui_base_layer_t;

typedef struct gui_fixed_layer {
    gui_layer_type_t type;
    ca_layer* inner;
} gui_fixed_layer_t;

typedef struct gui_scroll_layer {
    gui_layer_type_t type;
    ca_layer* inner;
    array_t* backing_layers;
    Point scroll_offset;
    uint32_t max_y;
} gui_scroll_layer_t;

typedef union gui_layer {
    gui_base_layer_t base;
    gui_fixed_layer_t fixed_layer;
    gui_scroll_layer_t scroll_layer;
} gui_layer_t;

gui_layer_t* gui_layer_create(gui_layer_type_t type, Size max_size);
void gui_layer_teardown(gui_layer_t* layer);

Rect gui_layer_blit_layer(gui_layer_t* dest, gui_layer_t* src, Rect dest_frame, Rect src_frame);
void gui_layer_draw_rect(gui_layer_t* layer, Rect r, Color color, int thickness);
void gui_layer_draw_line(gui_layer_t* layer, Line line, Color color, int thickness);
void gui_layer_draw_circle(gui_layer_t* layer, Circle circle, Color color, int thickness);
void gui_layer_draw_char(gui_layer_t* layer, char ch, int x, int y, Color color, Size font_size);

#endif