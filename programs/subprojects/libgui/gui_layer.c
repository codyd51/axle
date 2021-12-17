#include <stdint.h>
#include <stdlib.h>

#include <libutils/assert.h>
#include <libutils/array.h>

#include <libagx/font/font.h>
#include "utils.h"
#include "gui_elem.h"
#include "gui_layer.h"

ca_layer* _get_raw_layer(gui_layer_t* layer) {
    if (layer->base.type == GUI_FIXED_LAYER) {
        return layer->fixed_layer.inner;
    }
    else if (layer->base.type == GUI_SCROLL_LAYER) {
        ca_layer* s = layer->scroll_layer.inner;
        return s;
    }
    assert(false, "Unknown layer type");
    return NULL;
}

Point _adjust_point(gui_layer_t* layer, Point point) {
    return point;
}

typedef struct scroll_layer_backing_layer {
    Point origin;
    ca_layer* layer;
} scroll_layer_backing_layer_t;

bool rect_contains_rect(Rect a, Rect b) {
    return rect_max_x(b) <= rect_max_x(a) &&
           rect_min_x(b) >= rect_min_x(a) &&
           rect_min_y(b) >= rect_min_y(a) && 
           rect_max_y(b) <= rect_max_y(a);
}

array_t* rect_diff(Rect bg, Rect fg) {
    array_t* out = array_create(6);

    // Split by left edge if it's between the subject's left and right edges
    if (rect_min_x(fg) > rect_min_x(bg) && rect_min_x(fg) <= rect_max_x(bg)) {
        // Span from the background's left up to the foreground
        int diff = rect_min_x(fg) - rect_min_x(bg);
        Rect* r = calloc(1, sizeof(Rect));
        *r = rect_make(
            point_make(
                rect_min_x(bg),
                rect_min_y(bg)
            ),
            size_make(
                diff - 1,
                bg.size.height
            )
        );
        array_insert(out, r);
        //Shrink the backgrouund to exclude the split portion
        bg.origin.x += diff;
        bg.size.width -= diff;
        printf("Left overhang %d\n", diff);
    }

    // Split by bottom edge if it's between the top and bottom edge
    if (rect_min_y(fg) >= rect_min_y(bg) && rect_max_y(fg) < rect_max_y(bg)) {
        int diff = rect_max_y(bg) - rect_max_y(fg);
        Rect* r = calloc(1, sizeof(Rect));
        *r = rect_make(
            point_make(
                rect_min_x(bg),
                rect_max_y(fg) + 1
            ),
            size_make(
                bg.size.width,
                diff
            )
        );
        array_insert(out, r);
        //Shrink the background to exclude the split portion
        bg.size.height -= diff;
        printf("Bottom overhang %d\n", diff);
    }
    return out;
}

static void _gui_layer_draw_rect(gui_layer_t* layer, Rect r, Color c, int thickness) {
    ca_layer* dest = _get_raw_layer(layer);
    r.origin = _adjust_point(layer, r.origin);

    draw_rect(
        (ca_layer*)dest,
        r,
        c,
        thickness
    );
}

static Rect _rect_intersect(Rect a, Rect b) {
    Rect out = rect_zero();

    out.origin.x = max(rect_min_x(a), rect_min_x(b));
    out.origin.y = max(rect_min_y(a), rect_min_y(b));
    int dx = min(rect_max_x(a), rect_max_x(b)) - max(rect_min_x(a), rect_min_x(b));
    int dy = min(rect_max_y(a), rect_max_y(b)) - max(rect_min_y(a), rect_min_y(b));
    out.size.width = dx;
    out.size.height = dy;

    return out;
}

void _scroll_layer_expand_to_contain_rect(gui_layer_t* layer, Rect r) {
    array_t* backing_layers = layer->scroll_layer.backing_layers;

    Rect bounding_rect = rect_make(
        point_make(INT32_MAX, INT32_MAX), 
        size_make(0, 0)
    );
    for (uint32_t i = 0; i < backing_layers->size; i++) {
        scroll_layer_backing_layer_t* backing_layer = array_lookup(backing_layers, i);
        printf("Backing layer (%d %d, %d %d)\n", backing_layer->origin.x, backing_layer->origin.y, backing_layer->layer->size.width, backing_layer->layer->size.height);

        bounding_rect.origin.x = min(bounding_rect.origin.x, backing_layer->origin.x);
        bounding_rect.origin.y = min(bounding_rect.origin.y, backing_layer->origin.y);
        bounding_rect.size.width = max(bounding_rect.size.width, backing_layer->layer->size.width);
        bounding_rect.size.height = max(bounding_rect.size.height, backing_layer->layer->size.height);
    }
    printf("%s - Union of all backing layers\n", rect_print(bounding_rect));
    if (rect_contains_rect(bounding_rect, r)) {
        printf("Fully contained\n");
        return;
    }

    printf("Bounding rect does not fully contain rect:\n");
    printf("%s Bounding rect\n", rect_print(bounding_rect));
    printf("%s Rect\n", rect_print(r));
    printf("Not fully contained\n");
    Rect overhang = _rect_intersect(bounding_rect, r);
    array_t* splits = rect_diff(r, bounding_rect);

    uint32_t new_max_y = rect_max_y(bounding_rect);

    for (uint32_t i = 0; i < splits->size; i++) {
        Rect* split = array_lookup(splits, i);
        printf("%s Split %d\n", rect_print(*split), i);
        new_max_y = max(new_max_y, rect_max_y(*split));
    }

    printf("New max y %d bounding rect height %d\n", new_max_y, bounding_rect.size.height * 2);
    uint32_t new_layer_height = max(new_max_y, bounding_rect.size.height * 2);
    printf("New layer height: %d\n", new_layer_height);

    scroll_layer_backing_layer_t* new_layer = calloc(1, sizeof(scroll_layer_backing_layer_t));
    new_layer->origin = point_make(
        0,
        rect_max_y(bounding_rect) + 1
    );
    new_layer->layer = create_layer(
        size_make(
            bounding_rect.size.width, 
            new_layer_height
        )
    );
    array_insert(layer->scroll_layer.backing_layers, new_layer);
    layer->scroll_layer.max_y = new_layer->origin.y + new_layer_height;

    printf("%s --- Finished expansion to hold draw_rect\n", rect_print(r));
    for (uint32_t i = 0; i < splits->size; i++) {
        Rect* s = array_lookup(splits, i);
        free(s);
    }
    // TODO(PT): array_destroy
    free(splits);

    for (uint32_t i = 0; i < backing_layers->size; i++) {
        scroll_layer_backing_layer_t* backing_layer = array_lookup(backing_layers, i);
        Rect backing_layer_frame = rect_make(
            backing_layer->origin,
            backing_layer->layer->size
        );
        draw_rect(backing_layer->layer, rect_make(point_zero(), backing_layer->layer->size), color_rand(), 2);
    }
}

void gui_layer_draw_rect(gui_layer_t* layer, Rect r, Color color, int thickness) {
    if (layer->base.type == GUI_SCROLL_LAYER) {
        /*
        //layer->scroll_layer.max_y = max(layer->scroll_layer.max_y, rect_max_y(r));
        _scroll_layer_expand_to_contain_rect(layer, r);

        array_t* backing_layers = layer->scroll_layer.backing_layers;
        for (uint32_t i = 0; i < backing_layers->size; i++) {
            scroll_layer_backing_layer_t* backing_layer = array_lookup(backing_layers, i);
            Rect backing_layer_frame = rect_make(
                backing_layer->origin,
                backing_layer->layer->size
            );
            if (!rect_intersects(backing_layer_frame, r)) {
                continue;
            }
            Rect intersection = _rect_intersect(backing_layer_frame, r);
            printf("%s Intersection backing layer\n", rect_print(backing_layer_frame));
            printf("%s Intersection r\n", rect_print(r));
            printf("%s Intersection result\n", rect_print(intersection));
            Point offset = point_make(intersection.origin.x - r.origin.x, intersection.origin.y - r.origin.y);

            draw_rect(
                backing_layer->layer,
                rect_make(
                    point_make(
                        r.origin.x + offset.x,
                        r.origin.y + offset.y
                    ),
                    intersection.size
                ),
                color,
                thickness
            );
        }
        return;
        */
    }
    _gui_layer_draw_rect(layer,r , color, thickness);
}

void gui_layer_draw_line(gui_layer_t* layer, Line line, Color color, int thickness) {
    if (layer->base.type == GUI_SCROLL_LAYER) {
        uint32_t max_point = max(line.p1.y, line.p2.y);
        layer->scroll_layer.max_y = max(layer->scroll_layer.max_y, max_point);
    }

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

void gui_layer_draw_circle(gui_layer_t* layer, Circle circle, Color color, int thickness) {
    if (layer->base.type == GUI_SCROLL_LAYER) {
        layer->scroll_layer.max_y = max(layer->scroll_layer.max_y, circle.center.y + circle.radius);
    }

    ca_layer* dest = _get_raw_layer(layer);
    circle.center = _adjust_point(layer, circle.center);

    draw_circle(
        dest,
        circle,
        color,
        thickness
    );
}

void gui_layer_draw_char(gui_layer_t* layer, char ch, int x, int y, Color color, Size font_size) {
    if (layer->base.type == GUI_SCROLL_LAYER) {
        layer->scroll_layer.max_y = max(layer->scroll_layer.max_y, y + font_size.height);

        /*
        Rect r = rect_make(point_make(x, y), font_size);
        _scroll_layer_expand_to_contain_rect(layer, r);
        array_t* backing_layers = layer->scroll_layer.backing_layers;
        for (uint32_t i = 0; i < backing_layers->size; i++) {
            scroll_layer_backing_layer_t* backing_layer = array_lookup(backing_layers, i);
            Rect backing_layer_frame = rect_make(
                backing_layer->origin,
                backing_layer->layer->size
            );
            if (!rect_intersects(backing_layer_frame, r)) {
                continue;
            }
            Rect intersection = _rect_intersect(backing_layer_frame, r);
            printf("%s Intersection backing layer\n", rect_print(backing_layer_frame));
            printf("%s Intersection r\n", rect_print(r));
            printf("%s Intersection result\n", rect_print(intersection));
            Point offset = point_make(intersection.origin.x - r.origin.x, intersection.origin.y - r.origin.y);

            draw_char(
                backing_layer->layer,
                ch,
                r.origin.x + offset.x,
                r.origin.y + offset.y,
                color,
                font_size
            );
        }
        return;
        */
    }

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

Rect gui_layer_blit_layer(gui_layer_t* dest_wrapper, gui_layer_t* src_wrapper, Rect dest_frame, Rect src_frame) {
    ca_layer* dest = _get_raw_layer(dest_wrapper);
    Rect blit_rect;
    if (src_wrapper->base.type == GUI_FIXED_LAYER) {
        blit_rect = blit_layer(
            dest,
            src_wrapper->fixed_layer.inner,
            dest_frame,
            src_frame
        );
    }
    else if (src_wrapper->base.type == GUI_SCROLL_LAYER) {
        src_frame.origin.x += src_wrapper->scroll_layer.scroll_offset.x;
        src_frame.origin.y += src_wrapper->scroll_layer.scroll_offset.y;

        /*
        printf("%s blit layer source rect\n", rect_print(src_frame));
        printf("%s blit layer dest  rect\n", rect_print(dest_frame));
        for (uint32_t i = 0; i < src_wrapper->scroll_layer.backing_layers->size; i++) {
            scroll_layer_backing_layer_t* backing_layer = array_lookup(src_wrapper->scroll_layer.backing_layers, i);
            Rect backing_layer_frame = rect_make(
                backing_layer->origin,
                backing_layer->layer->size
            );
            if (!rect_intersects(backing_layer_frame, src_frame)) {
                continue;
            }
            printf("%s Found backing layer %d with an intersection\n", rect_print(backing_layer_frame), i);
            Rect intersection = _rect_intersect(backing_layer_frame, src_frame);
            Point offset = point_make(intersection.origin.x - src_frame.origin.x, intersection.origin.y - src_frame.origin.y);
            printf("%s Intersection %d\n", rect_print(intersection), i);

            blit_layer(
                dest,
                backing_layer->layer,
                rect_make(
                    point_make(
                        dest_frame.origin.x + offset.x,
                        dest_frame.origin.y + offset.y
                    ),
                    intersection.size
                ),
                intersection
            );
        }
        */
        blit_rect = blit_layer(
            dest,
            src_wrapper->scroll_layer.inner,
            dest_frame,
            src_frame
        );

        dest_wrapper->scroll_layer.max_y = max(dest_wrapper->scroll_layer.max_y, rect_max_y(dest_frame));
    }
    else {
        assert(false, "Unknown layer type");
    }
    return blit_rect;
}

gui_layer_t* gui_layer_create(gui_layer_type_t type, Size max_size) {
    gui_layer_t* l = calloc(1, sizeof(gui_layer_t));
    l->base.type = type;

    if (type == GUI_FIXED_LAYER) {
        l->fixed_layer.inner = create_layer(max_size);
    }
    else if (type == GUI_SCROLL_LAYER) {
        l->scroll_layer.inner = create_layer(max_size);
        /*
        l->scroll_layer.backing_layers = array_create(32);
        scroll_layer_backing_layer_t* b = calloc(1, sizeof(scroll_layer_backing_layer_t));
        b->origin = point_zero();
        b->layer = l->scroll_layer.inner;
        array_insert(l->scroll_layer.backing_layers, b);
        */
    }
    else {
        assert(false, "Unknown layer type");
    }

    return l;
}

void gui_layer_teardown(gui_layer_t* layer) {
    if (layer->base.type == GUI_FIXED_LAYER) {
        layer_teardown(layer->fixed_layer.inner);
    }
    else if (layer->base.type == GUI_SCROLL_LAYER) {
        layer_teardown(layer->scroll_layer.inner);
    }
    free(layer);
}
