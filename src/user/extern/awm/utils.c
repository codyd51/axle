#include "utils.h"

Point point_translate(Point p, Rect r) {
    return point_make(
        p.x - rect_min_x(r),
        p.y - rect_min_y(r)
    );
}

Size screen_resolution(void) {
    return size_make(1920, 1080);
}

uint8_t screen_bytes_per_pixel(void) {
    return 4;
}

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
        //printf("Shrink left edge\n");
        // Span from the background's left up to the foreground
        int diff = rect_min_x(fg) - rect_min_x(bg);
        Rect* r = calloc(1, sizeof(Rect));
        *r = rect_make(
            point_make(
                rect_min_x(bg),
                rect_min_y(bg)
            ),
            size_make(
                diff,
                bg.size.height
            )
        );
        array_insert(out, r);
        //Shrink the backgrouund to exclude the split portion
        bg.origin.x += diff;
        bg.size.width -= diff;
    }

    // Split by top edge
    if ((rect_min_y(fg) > rect_min_y(bg) && rect_min_y(fg) <= rect_max_y(bg))) {
        //printf("Split by top edge\n");
        // Background top to foreground top
        int diff = rect_min_y(fg) - rect_min_y(bg);
        Rect* r = calloc(1, sizeof(Rect));
        *r = rect_make(
            point_make(
                rect_min_x(bg),
                rect_min_y(bg)
            ),
            size_make(
                bg.size.width, 
                diff
            )
        );
        array_insert(out, r);
        //Shrink the backgrouund to exclude the split portion
        bg.origin.y += diff;
        bg.size.height -= diff;
    }


    // Split by right edge
    if (rect_max_x(fg) > rect_min_x(bg) && rect_max_x(fg) <= rect_max_x(bg)) {
        //printf("Shrink right edge\n");
        int diff = rect_max_x(bg) - rect_max_x(fg);

        Rect* r = calloc(1, sizeof(Rect));
        *r = rect_make(
            point_make(
                rect_max_x(fg),
                rect_min_y(bg)
            ),
            size_make(
                diff,
                bg.size.height
            )
        );
        array_insert(out, r);
        bg.size.width -= diff;
    }

    // Split by bottom edge if it's between the top and bottom edge
    if (rect_max_y(fg) > rect_min_y(bg) && rect_max_y(fg) <= rect_max_y(bg)) {
        //printf("Shrink bottom edge\n");
        int diff = rect_max_y(bg) - rect_max_y(fg);
        Rect* r = calloc(1, sizeof(Rect));
        *r = rect_make(
            point_make(
                rect_min_x(bg),
                rect_max_y(fg)
            ),
            size_make(
                bg.size.width,
                diff
            )
        );
        array_insert(out, r);
        //Shrink the background to exclude the split portion
        bg.size.height -= diff;
    }

    // Cull zero-length rects
    /*
    for (int32_t i = out->size - 1; i >= 0; i--) {
        Rect* r = array_lookup()
    }
    */

    return out;
}

void rect_add(array_t* arr, Rect r) {
    Rect* rp = calloc(1, sizeof(Rect));
    rp->origin.x = r.origin.x;
    rp->origin.y = r.origin.y;
    rp->size.width = r.size.width;
    rp->size.height = r.size.height;
	array_insert(arr, rp);
}

array_t* update_occlusions(array_t* free_areas, Rect exclude_rect) {
    array_t* new_free_areas = array_create(free_areas->max_size);
    for (int32_t free_area_idx = 0; free_area_idx < free_areas->size; free_area_idx++) {
        Rect* free_area = array_lookup(free_areas, free_area_idx);
        if (!rect_intersects(*free_area, exclude_rect)) {
            array_insert(new_free_areas, free_area);
            continue;
        }

        array_t* occlusions = rect_diff(*free_area, exclude_rect);
        for (int occlusion_idx = 0; occlusion_idx < occlusions->size; occlusion_idx++) {
            Rect* r = array_lookup(occlusions, occlusion_idx);
            array_insert(new_free_areas, r);
        }
        array_destroy(occlusions);
    }
    array_destroy(free_areas);
    return new_free_areas;
}
