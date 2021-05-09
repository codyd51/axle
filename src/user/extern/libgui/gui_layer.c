#include <stdint.h>
#include <stdlib.h>

#include <agx/font/font.h>
#include "gui_layer.h"

void ca_scrolling_layer_blit(ca_scrolling_layer_t* source, Rect source_frame, ca_layer* dest, Rect dest_frame);

ca_layer* _get_raw_layer(gui_layer_t* layer) {
    if (layer->base.type == GUI_FIXED_LAYER) {
        return layer->fixed_layer.inner;
    }
    else if (layer->base.type == GUI_SCROLL_LAYER) {
        ca_scrolling_layer_t* s = layer->scroll_layer.inner;
        return s->layer;
    }
    assert(false, "Unknown layer type");
    return NULL;
}

Point _adjust_point(gui_layer_t* layer, Point point) {
    if (layer->base.type == GUI_SCROLL_LAYER) {
        ca_scrolling_layer_t* s = layer->scroll_layer.inner;
        return point;
        return point_make(
            point.x - s->scroll_offset.width,
            point.y - s->scroll_offset.height
        );
    }
    return point;
}

void gui_layer_draw_rect(gui_layer_t* layer, Rect r, Color color, int thickness) {
    ca_layer* dest = _get_raw_layer(layer);
    r.origin = _adjust_point(layer, r.origin);

    draw_rect(
        (ca_layer*)dest,
        r,
        color,
        thickness
    );
}

void gui_layer_draw_line(gui_layer_t* layer, Line line, Color color, int thickness) {
    ca_layer* dest = _get_raw_layer(layer);
    line.p1 = _adjust_point(layer, line.p1);
    line.p2 = _adjust_point(layer, line.p2);

    draw_line(
        dest,
        line,
        color,
        thickness
    );
}

void gui_layer_draw_char(gui_layer_t* layer, char ch, int x, int y, Color color, Size font_size) {
    ca_layer* dest = _get_raw_layer(layer);
    Point adjusted = _adjust_point(layer, point_make(x, y));
    draw_char(
        dest,
        ch,
        adjusted.x,
        adjusted.y,
        color,
        font_size
    );
}

void gui_layer_blit_layer(gui_layer_t* dest_wrapper, gui_layer_t* src_wrapper, Rect dest_frame, Rect src_frame) {
    ca_layer* dest = _get_raw_layer(dest_wrapper);
    if (src_wrapper->base.type == GUI_FIXED_LAYER) {
        blit_layer(
            dest,
            src_wrapper->fixed_layer.inner,
            dest_frame,
            src_frame
        );
    }
    else if (src_wrapper->base.type == GUI_SCROLL_LAYER) {
        ca_scrolling_layer_blit(
            src_wrapper->scroll_layer.inner,
            src_frame,
            dest,
            dest_frame
        );
    }
    else {
        assert(false, "Unknown layer type");
    }
}
