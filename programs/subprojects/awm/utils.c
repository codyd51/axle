#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <libutils/assert.h>
#include <file_server/file_server_messages.h>

static Size _screen_resolution = {0};
static int _screen_bytes_per_pixel = 0;
static uint32_t _screen_pixels_per_scanline = 0;

Point point_translate(Point p, Rect r) {
    return point_make(
        p.x - rect_min_x(r),
        p.y - rect_min_y(r)
    );
}

void _set_screen_resolution(Size s) {
    _screen_resolution = s;
}

Size screen_resolution(void) {
    return _screen_resolution;
}

uint8_t screen_bytes_per_pixel(void) {
    return _screen_bytes_per_pixel;
}

void _set_screen_bytes_per_pixel(uint8_t screen_bpp) {
    _screen_bytes_per_pixel = screen_bpp;
}

uint32_t screen_pixels_per_scanline(void) {
    return _screen_pixels_per_scanline;
}

void _set_screen_pixels_per_scanline(uint32_t px_per_scanline) {
    _screen_pixels_per_scanline = px_per_scanline;
}

bool rect_contains_rect(Rect a, Rect b) {
    return rect_max_x(b) <= rect_max_x(a) &&
           rect_min_x(b) >= rect_min_x(a) &&
           rect_min_y(b) >= rect_min_y(a) && 
           rect_max_y(b) <= rect_max_y(a);
}

array_t* rect_diff(Rect bg, Rect fg) {
    //assert(bg.size.width > 0 && bg.size.height > 0 && fg.size.width > 0 && fg.size.height > 0, "Invalid input to rect_diff");
    array_t* out = array_create(5);

    // Split by left edge if it's between the subject's left and right edges
    if (rect_min_x(fg) > rect_min_x(bg) && rect_min_x(fg) <= rect_max_x(bg)) {
        //printf("Shrink left edge\n");
        // Span from the background's left up to the foreground
        int diff = rect_min_x(fg) - rect_min_x(bg);
        //assert(diff > 0, "Diff was 0");
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
    /*
    if (bg.size.width == 0 || bg.size.height == 0) {
        goto end;
    }
    */

    // Split by top edge
    if ((rect_min_y(fg) > rect_min_y(bg) && rect_min_y(fg) <= rect_max_y(bg))) {
        //printf("Split by top edge\n");
        // Background top to foreground top
        int diff = rect_min_y(fg) - rect_min_y(bg);
        //assert(diff > 0, "Diff was 0");
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
    /*
    if (bg.size.width == 0 || bg.size.height == 0) {
        goto end;
    }
    */

    // Split by right edge
    if (rect_max_x(fg) > rect_min_x(bg) && rect_max_x(fg) < rect_max_x(bg)) {
        //printf("Shrink right edge\n");
        int diff = rect_max_x(bg) - rect_max_x(fg);
        //assert(diff > 0, "Diff was 0");

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
    /*
    if (bg.size.width == 0 || bg.size.height == 0) {
        goto end;
    }
    */

    // Split by bottom edge if it's between the top and bottom edge
    if (rect_max_y(fg) > rect_min_y(bg) && rect_max_y(fg) < rect_max_y(bg)) {
        //printf("Shrink bottom edge\n");
        int diff = rect_max_y(bg) - rect_max_y(fg);
        //assert(diff > 0, "Diff was 0");
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
    /*
    if (bg.size.width == 0 || bg.size.height == 0) {
        goto end;
    }
    */

    // Cull zero-length rects
    /*
    for (int32_t i = out->size - 1; i >= 0; i--) {
        Rect* r = array_lookup()
    }
    */

//end:
    return out;
}

void rect_add(array_t* arr, Rect r) {
    if (r.size.width == 0 || r.size.height == 0) {
        assert(false, "Found zero-sized rect_add");
    }
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
            assert(free_area->size.width > 0 && free_area->size.height > 0, "Free area was zero-sized");
            array_insert(new_free_areas, free_area);
            continue;
        }

        // TODO(PT): Add asserts to rect_diff?
        array_t* occlusions = rect_diff(*free_area, exclude_rect);
        for (int occlusion_idx = 0; occlusion_idx < occlusions->size; occlusion_idx++) {
            Rect* r = array_lookup(occlusions, occlusion_idx);
            assert(r->size.width > 0 && r->size.height > 0, "New free area was zero-sized");
            array_insert(new_free_areas, r);
        }
        array_destroy(occlusions);
    }
    array_destroy(free_areas);
    return new_free_areas;
}

image_t* load_image(const char* image_name) {
	printf("AWM sending read file request for %s...\n", image_name);
    file_server_read_t read = {0};
    read.event = FILE_SERVER_READ_FILE_EVENT;
	snprintf(read.path, sizeof(read.path), "%s", image_name);
	amc_message_send(FILE_SERVER_SERVICE_NAME, &read, sizeof(file_server_read_t));

	printf("AWM awaiting file read response for %s...\n", image_name);
	amc_message_t* file_data_msg;
    amc_message_await__u32_event(FILE_SERVER_SERVICE_NAME, FILE_SERVER_READ_FILE_EVENT, &file_data_msg);

	printf("AWM got response for %s!\n", image_name);
	file_server_read_response_t* resp = (file_server_read_response_t*)&file_data_msg->body;
	return image_parse(resp->len, resp->data);
}
