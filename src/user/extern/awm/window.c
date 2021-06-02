#include <stdlib.h>
#include <stdlibadd/array.h>

#include "window.h"
#include "awm_messages.h"
#include "utils.h"

// Sorted by Z-index
#define MAX_WINDOW_COUNT 64
array_t* windows = NULL;
array_t* windows_to_fetch_this_cycle = NULL;
array_t* windows_to_composite_this_cycle = NULL;

image_t* _g_title_bar_image = NULL;
image_t* _g_title_bar_x_unfilled = NULL;
image_t* _g_title_bar_x_filled = NULL;

user_window_t* window_move_to_top(user_window_t* window) {
	uint32_t idx = array_index(windows, window);
	array_remove(windows, idx);

	// TODO(PT): array_insert_at_index
	array_t* a = windows;
	for (int32_t i = a->size; i >= 0; i--) {
		a->array[i] = a->array[i - 1];
	}
	a->array[0] = window;
	a->size += 1;
    windows_invalidate_drawable_regions_in_rect(window->frame);
	return window;
}

user_window_t* windows_get_bottom_window(void) {
    if (windows->size == 0) {
        return NULL;
    }
    return array_lookup(windows, windows->size - 1);
}

user_window_t* windows_get_top_window(void) {
    if (windows->size == 0) {
        return NULL;
    }
    return array_lookup(windows, 0);
}

user_window_t* window_containing_point(Point point) {
	for (int i = 0; i < windows->size; i++) {
		user_window_t* window = array_lookup(windows, i);
		if (rect_contains_point(window->frame, point)) {
            return window;
        }
    }
    return NULL;
}

user_window_t* window_with_service_name(const char* service_name) {
	for (int i = 0; i < windows->size; i++) {
		user_window_t* window = array_lookup(windows, i);
		if (!strncmp(window->owner_service, service_name, AMC_MAX_SERVICE_NAME_LEN)) {
			return window;
		}
	}
	return NULL;
}

void window_handle_left_click(user_window_t* window,  Point mouse_within_window) {
    assert(window != NULL, "Expected non-NULL window");

    // Is the mouse within the window's content view?
    if (rect_contains_point(window->content_view->frame, mouse_within_window)) {
        Point mouse_within_content_view = point_make(
            mouse_within_window.x - rect_min_x(window->content_view->frame),
            mouse_within_window.y - rect_min_y(window->content_view->frame)
        );
        awm_mouse_left_click_msg_t msg = {0};
        msg.event = AWM_MOUSE_LEFT_CLICK;
        msg.click_point = mouse_within_content_view;
        amc_message_construct_and_send(window->owner_service, &msg, sizeof(msg));
    }
}

void window_handle_left_click_ended(user_window_t* window,  Point mouse_within_window) {
    assert(window != NULL, "Expected non-NULL window");

    // Is the mouse within the window's content view?
    if (rect_contains_point(window->content_view->frame, mouse_within_window)) {
        Point mouse_within_content_view = point_make(
            mouse_within_window.x - rect_min_x(window->content_view->frame),
            mouse_within_window.y - rect_min_y(window->content_view->frame)
        );
		amc_msg_u32_3__send(
            window->owner_service, 
            AWM_MOUSE_LEFT_CLICK_ENDED, 
            mouse_within_content_view.x, 
            mouse_within_content_view.y
        );
    }
}

void window_handle_mouse_entered(user_window_t* window) {
    assert(window != NULL, "Expected non-NULL window");

	// Inform the window the mouse has just entered it
	amc_msg_u32_1__send(window->owner_service, AWM_MOUSE_ENTERED);
}

void window_handle_mouse_exited(user_window_t* window) {
    assert(window != NULL, "Expected non-NULL window");

	// Inform the window the mouse has just exited it
	amc_msg_u32_1__send(window->owner_service, AWM_MOUSE_EXITED);
}

void window_handle_mouse_moved(user_window_t* window, Point mouse_within_window) {
    assert(window != NULL, "Expected non-NULL window");

    // Is the mouse within the window's content view?
    if (rect_contains_point(window->content_view->frame, mouse_within_window)) {
        Point mouse_within_content_view = point_make(
            mouse_within_window.x - rect_min_x(window->content_view->frame),
            mouse_within_window.y - rect_min_y(window->content_view->frame)
        );
        amc_msg_u32_3__send(
            window->owner_service, 
            AWM_MOUSE_MOVED, 
            mouse_within_content_view.x, 
            mouse_within_content_view.y
        );
    }
}

void window_handle_keyboard_event(user_window_t* window, uint32_t event, uint32_t key) {
    assert(window != NULL, "Expected non-NULL window");

    amc_msg_u32_2__send(window->owner_service, event, key);
}

static void _window_fetch_framebuf(user_window_t* window) {
    assert(window != NULL, "Expected non-NULL window");

	window->has_done_first_draw = true;
	blit_layer(
		window->layer, 
		window->content_view->layer, 
		window->content_view->frame, 
		rect_make(point_zero(), window->content_view->frame.size)
	);
}

void window_queue_fetch(user_window_t* window) {
    if (array_index(windows_to_fetch_this_cycle, window) == -1) {
        //printf("Ready for redraw: %s\n", window->owner_service);
        array_insert(windows_to_fetch_this_cycle, window);
    }
    window_queue_composite(window);
}

void window_queue_composite(user_window_t* window) {
    if (array_index(windows_to_composite_this_cycle, window) == -1) {
        array_insert(windows_to_composite_this_cycle, window);
    }
}

void window_queue_extra_draw(user_window_t* window, Rect extra) {
    rect_add(window->extra_draws_this_cycle, extra);
}

void window_request_close(user_window_t* window) {

}

static void _write_window_title(user_window_t* window) {
	Point mid = point_make(
		window->frame.size.width / 2.0, 
		(WINDOW_TITLE_BAR_VISIBLE_HEIGHT) / 2.0
	);
	Size font_size = size_make(8, 12);
	uint32_t len = strlen(window->title);
	Point origin = point_make(
		mid.x - ((font_size.width * len) / 2.0),
		mid.y - (font_size.height / 2.0) - 1
	);
	for (uint32_t i = 0; i < len; i++) {
		draw_char(
			window->layer,
			window->title[i],
			origin.x + (font_size.width * i),
			origin.y,
			color_make(50, 50, 50),
			font_size
		);
	}
}

void window_redraw_title_bar(user_window_t* window, bool close_button_hovered) {
	if (!_g_title_bar_image) {
		printf("No images yet...\n");
		return;
	}

	Size title_bar_size = size_make(window->frame.size.width, WINDOW_TITLE_BAR_HEIGHT);
	image_render_to_layer(
		_g_title_bar_image, 
		window->layer, 
		rect_make(
			point_zero(), 
			size_make(title_bar_size.width, WINDOW_TITLE_BAR_VISIBLE_HEIGHT)
		)
	);

	//bool is_x_filled = g_mouse_state.active_window == window && (g_mouse_state.is_prospective_window_move || g_mouse_state.is_moving_top_window);
	image_t* x_image = (close_button_hovered) ? _g_title_bar_x_filled : _g_title_bar_x_unfilled;
	uint32_t icon_height = x_image->size.height;
	window->close_button_frame = rect_make(
		point_make(icon_height * 0.75, icon_height * 0.275), 
		x_image->size
	);
	image_render_to_layer(
		x_image, 
		window->layer, 
		window->close_button_frame
	);

	// Draw window title
	_write_window_title(window);
}

void windows_fetch_queued_windows(void) {
    for (int32_t i = 0; i < windows_to_fetch_this_cycle->size; i++) {
        user_window_t* window_to_update = array_lookup(windows_to_fetch_this_cycle, i);
        _window_fetch_framebuf(window_to_update);
        //array_remove(windows_to_fetch_this_cycle, 0);
    }
}

void windows_clear_queued_windows(void) {
    for (int32_t i = windows_to_fetch_this_cycle->size - 1; i >= 0; i--) {
        array_remove(windows_to_fetch_this_cycle, i);
    }
    for (int32_t i = windows_to_composite_this_cycle->size - 1; i >= 0; i--) {
        array_remove(windows_to_composite_this_cycle, i);
    }
}

void windows_init(void) {
    windows = array_create(MAX_WINDOW_COUNT);
    windows_to_fetch_this_cycle = array_create(MAX_WINDOW_COUNT);
    windows_to_composite_this_cycle = array_create(MAX_WINDOW_COUNT);
}

void windows_fetch_resource_images(void) {
	_g_title_bar_image = load_image("titlebar7.bmp");
	_g_title_bar_x_filled = load_image("titlebar_x_filled2.bmp");
	_g_title_bar_x_unfilled = load_image("titlebar_x_unfilled2.bmp");
    for (int32_t i = 0; i < windows->size; i++) {
        user_window_t* w = array_lookup(windows, i);
        window_redraw_title_bar(w, false);
    }
}

user_window_t* window_create(const char* owner_service, uint32_t width, uint32_t height) {
	user_window_t* window = calloc(1, sizeof(user_window_t));
	array_insert(windows, window);

	window->drawable_rects = array_create(32);
	window->extra_draws_this_cycle = array_create(32);

	// Shared layer is size of the screen to allow window resizing
    Size res = screen_resolution();
	uint32_t shmem_size = res.width * res.height * screen_bytes_per_pixel(); 
	uint32_t shmem_local = 0;
	uint32_t shmem_remote = 0;

	printf("Creating framebuffer for %s\n", owner_service);
	uint32_t local_buffer;
	uint32_t remote_buffer;
	amc_shared_memory_create(
		owner_service, 
		shmem_size, 
		&shmem_local, 
		&shmem_remote
	);

	// Place the window in the center of the screen
	Point origin = point_make(
		(res.width / 2) - (width / 2),
		(res.height / 2) - (height / 2)
	);
	window->frame = rect_make(origin, size_zero());
	window->layer = create_layer(res);

	view_t* content_view = calloc(1, sizeof(view_t));
	content_view->layer = calloc(1, sizeof(ca_layer));
	content_view->layer->size = res;
	content_view->layer->raw = (uint8_t*)shmem_local;
	content_view->layer->alpha = 1.0;
	window->content_view = content_view;

	// Copy the owner service name as we don't own it
	window->owner_service = strndup(owner_service, AMC_MAX_SERVICE_NAME_LEN);

	// Configure the title text box
	// The size will be reset by window_size()
	window->title = strndup(window->owner_service, strlen(window->owner_service));
	window_redraw_title_bar(window, false);

	// Make the window a bit bigger than the user requested to accomodate for decorations
	int full_window_width = width + (WINDOW_BORDER_MARGIN * 2);
	Size title_bar_size = size_make(full_window_width, WINDOW_TITLE_BAR_HEIGHT);
	Size full_window_size = size_make(
		full_window_width, 
		// We only need to add the border margin on the bottom edge
		// The top edge does not have a border margin
		height + title_bar_size.height + WINDOW_BORDER_MARGIN
	);

	// Make the new window show up on top
	window_move_to_top(window);

	// Now that we've configured the initial window state on our end, 
	// provide the buffer to the client
	printf("AWM made shared framebuffer for %s\n", owner_service);
	printf("\tAWM    memory: 0x%08x - 0x%08x\n", shmem_local, shmem_local + shmem_size);
	printf("\tRemote memory: 0x%08x - 0x%08x\n", shmem_remote, shmem_remote + shmem_size);
	amc_msg_u32_2__send(owner_service, AWM_CREATED_WINDOW_FRAMEBUFFER, shmem_remote);
	// Inform the window of its initial size
	_window_resize(window, full_window_size, true);

    return window;
}

void window_destroy(user_window_t* window) {
    assert(window != NULL, "Expected non-NULL window");

    int32_t i = array_index(windows, window);
    assert(i >= 0, "Window not found");
    array_remove(windows, i);

    i = array_index(windows_to_fetch_this_cycle, window);
    if (i >= 0) {
        array_remove(windows_to_fetch_this_cycle, i);
    }
    i = array_index(windows_to_composite_this_cycle, window);
    if (i >= 0) {
        array_remove(windows_to_composite_this_cycle, i);
    }

	layer_teardown(window->layer);

	// Special 'virtual' layer that doesn't need its internal buffer freed (because it's backed by shared memory)
	free(window->content_view->layer);
	free(window->content_view);

	free(window->owner_service);
	free(window->title);

	free(window);
}

array_t* windows_all(void) {
    return windows;
}

array_t* windows_queued(void) {
    return windows_to_fetch_this_cycle;
}

array_t* windows_get_ready_to_composite_array(void) {
    return windows_to_composite_this_cycle;
}

void windows_invalidate_drawable_regions_in_rect(Rect r) {
    for (int32_t i = windows->size - 1; i >= 0; i--) {
        user_window_t* window = array_lookup(windows, i);
        if (!rect_intersects(window->frame, r)) {
            continue;
        }

        for (int32_t i = window->drawable_rects->size - 1; i >= 0; i--) {
            Rect* r = array_lookup(window->drawable_rects, i);
            free(r);
            array_remove(window->drawable_rects, i);
        }
        rect_add(window->drawable_rects, window->frame);

        for (int32_t j = i - 1; j >= 0; j--) {
            user_window_t* occluding_window = array_lookup(windows, j);
            if (!rect_intersects(occluding_window->frame, window->frame)) {
                continue;
            }
            window->drawable_rects = update_occlusions(window->drawable_rects, occluding_window->frame);
            if (!window->drawable_rects->size) {
                break;
            }
        }
        if (window->drawable_rects->size) {
            window_queue_composite(window);
        }
        else {
            //printf("Will not composite %s because it's fully occluded\n", window->owner_service);
        }
    }
}

void windows_composite(ca_layer* dest, Rect updated_rect) {
    //printf("Update rect: (%d, %d), (%d, %d)\n", rect_min_x(updated_rect), rect_min_y(updated_rect), updated_rect.size.width, updated_rect.size.height);

    for (uint32_t i = 0; i < windows->size; i++) {
        user_window_t* window = array_lookup(windows, i);
        Rect* r = calloc(1, sizeof(Rect));
        *r = window->frame;
        array_insert(window->drawable_rects, r);
    }

    for (int i = 0; i < windows->size; i++) {
        user_window_t* window = array_lookup(windows, i);
        if (!window->has_done_first_draw) {
            continue;
        }

        for (int outer_rect_idx = 0; outer_rect_idx < window->drawable_rects->size; outer_rect_idx++) {
            Rect* outer_rect_ptr = array_lookup(window->drawable_rects, outer_rect_idx);
            Rect outer_rect = *outer_rect_ptr;

            for (int j = i + 1; j < windows->size; j++) {
                user_window_t* bg_window = array_lookup(windows, j);
                Color c = color_rand();

                array_t* new_drawable_rects = array_create(bg_window->drawable_rects->max_size);
                for (int k = 0; k < bg_window->drawable_rects->size; k++) {
                    Rect* inner_rect_ptr = array_lookup(bg_window->drawable_rects, k);
                    Rect inner_rect = *inner_rect_ptr;
                    if (!rect_intersects(outer_rect, inner_rect)) {
                        array_insert(new_drawable_rects, inner_rect_ptr);
                    }
                    else {
                        array_t* splits = rect_diff(inner_rect, outer_rect);
                        for (int split_idx = 0; split_idx < splits->size; split_idx++) {
                            Rect* r = array_lookup(splits, split_idx);
                            //printf("%d %d %d %d\n", r->origin.x, r->origin.y, r->size.width, r->size.height);
                            array_insert(new_drawable_rects, r);
                        }
                        array_destroy(splits);

                        free(inner_rect_ptr);
                    }
                }
                array_destroy(bg_window->drawable_rects);
                bg_window->drawable_rects = new_drawable_rects;
            }
        }

        blit_layer(
            dest, 
            window->layer, 
            window->frame, 
            rect_make(
                point_zero(), 
                size_make(
                    window->frame.size.width,
                    WINDOW_TITLE_BAR_VISIBLE_HEIGHT
                )
            )
        );
        blit_layer(
            dest, 
            window->layer, 
            rect_make(
                point_make(
                    window->frame.origin.x,
                    window->frame.origin.y + WINDOW_TITLE_BAR_HEIGHT
                ),
                size_make(
                    window->frame.size.width,
                    window->frame.size.height - WINDOW_TITLE_BAR_HEIGHT
                )
            ),
            rect_make(
                point_make(
                    0,
                    WINDOW_TITLE_BAR_HEIGHT
                ), 
                size_make(
                    window->frame.size.width,
                    window->frame.size.height - WINDOW_TITLE_BAR_HEIGHT
                )
            )
        );
    }
    /*
    draw_rect(dest, updated_rect, color_red(), 1);

    for (int32_t i = windows->size - 1; i >= 0; i--) {
        user_window_t* window = array_lookup(windows, i);
        //printf("%s has %d drawable rects\n", window->owner_service, window->drawable_rects->size);
        for (int j = 0; j < window->drawable_rects->size; j++) {
            Rect* r = array_lookup(window->drawable_rects, j);
            if (!rect_intersects(*r, updated_rect)) {
                continue;
            }

            uint32_t offset_x = r->origin.x - rect_min_x(window->frame);
            uint32_t offset_y = r->origin.y - rect_min_y(window->frame);
            blit_layer(
                dest, 
                window->layer, 
                *r, 
                rect_make(
                    point_make(offset_x, offset_y), 
                    r->size
                )
            );
            draw_rect(dest, *r, color_rand(), 2);
        }
    }
    */

    for (uint32_t i = 0; i < windows->size; i++) {
        user_window_t* window = array_lookup(windows, i);
        for (int32_t j = window->drawable_rects->size - 1; j >= 0; j--) {
            Rect* r = array_lookup(window->drawable_rects, j);
            free(r);
            array_remove(window->drawable_rects, j);
        }
    }
}
