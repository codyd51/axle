#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <libutils/assert.h>
#include <libutils/array.h>

#include <libamc/libamc.h>
#include <libagx/lib/gfx.h>
#include <kernel/core_commands.h>

#include "window.h"
#include "awm_messages.h"
#include "utils.h"
#include "math.h"
#include "animations.h"

typedef struct desktop_shortcuts_state {
    array_t* shortcuts;
    Point last_shortcut_origin;
    array_t* grid_slots;
} desktop_shortcuts_state_t;

// Sorted by Z-index
#define MAX_WINDOW_COUNT 64
array_t* desktop_views = NULL;

// Two windows arrays. 
// One contains all windows, including windows without Z-order (such as dock and status bar)
array_t* windows = NULL;
// The other contains draggable windows
array_t* windows_with_z_order = NULL;
array_t* windows_without_z_order = NULL;

array_t* windows_to_fetch_this_cycle = NULL;
array_t* views_to_composite_this_cycle = NULL;

image_t* _g_title_bar_image = NULL;
image_t* _g_title_bar_x_unfilled = NULL;
image_t* _g_title_bar_x_filled = NULL;
image_t* _g_title_bar_minimize_filled = NULL;
image_t* _g_title_bar_minimize_unfilled = NULL;
image_t* _g_executable_image = NULL;

static desktop_shortcuts_state_t _g_desktop_shortcuts_state = {0};

static view_t* _g_minimized_window_preview = NULL;

bool window_is_in_z_order(user_window_t* window) {
	int32_t idx = array_index(windows_with_z_order, window);
    return idx >= 0;
}

user_window_t* window_move_to_top(user_window_t* window) {
	int32_t idx = array_index(windows_with_z_order, window);
    // Is this window in the Z-order hierarchy?
    if (idx < 0) {
        printf("Window for %s is outside the Z-order hierarchy, ignoring request to move to top");
        return NULL;
    }
	array_remove(windows_with_z_order, idx);

	// TODO(PT): array_insert_at_index
	array_t* a = windows_with_z_order;
	for (int32_t i = a->size; i >= 0; i--) {
		a->array[i] = a->array[i - 1];
	}
	a->array[0] = window;
	a->size += 1;
    windows_invalidate_drawable_regions_in_rect(window->frame);
	return window;
}

user_window_t* windows_get_bottom_window(void) {
    if (windows_with_z_order->size == 0) {
        return NULL;
    }
    return array_lookup(windows_with_z_order, windows_with_z_order->size - 1);
}

user_window_t* windows_get_top_window(void) {
    if (windows_with_z_order->size == 0) {
        return NULL;
    }
    return array_lookup(windows_with_z_order, 0);
}

user_window_t* window_containing_point(Point point, bool ignore_zombie_windows) {
    // First, search the windows above the Z-order
	for (int i = 0; i < windows_without_z_order->size; i++) {
		user_window_t* window = array_lookup(windows_without_z_order, i);
        if (ignore_zombie_windows && window->remote_process_died) {
            continue;
        }
		if (rect_contains_point(window->frame, point)) {
            return window;
        }
    }

	for (int i = 0; i < windows_with_z_order->size; i++) {
		user_window_t* window = array_lookup(windows_with_z_order, i);
        if (ignore_zombie_windows && window->remote_process_died) {
            continue;
        }
		if (rect_contains_point(window->frame, point)) {
            return window;
        }
    }
    return NULL;
}

desktop_shortcut_t* desktop_shortcut_containing_point(Point point) {
	for (int i = 0; i < _g_desktop_shortcuts_state.shortcuts->size; i++) {
        desktop_shortcut_t* shortcut = array_lookup(_g_desktop_shortcuts_state.shortcuts, i);
        if (rect_contains_point(shortcut->view->frame, point)) {
            return shortcut;
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

void window_initiate_minimize(user_window_t* window) {
    awm_dock_window_minimize_requested_event_t minimize_event = {
        .event = AWM_DOCK_WINDOW_MINIMIZE_REQUESTED,
        .window_id = window->window_id
    };
    amc_message_send(AWM_DOCK_SERVICE_NAME, &minimize_event, sizeof(minimize_event));
}

user_window_t* window_with_id(uint32_t window_id) {
    // Find the window with the specified ID
    for (uint32_t i = 0; i < windows->size; i++) {
        user_window_t* window = array_lookup(windows, i);
        if (window->window_id == window_id) {
            return window;
        }
    }
    return NULL;
}

void window_minimize_from_message(awm_dock_window_minimize_with_info_event_t* event) {
    Rect r = event->task_view_frame;
    printf("Continuing minimize! Got info about window, ID %d, frame %d %d %d %d\n", event->window_id, r.origin.x, r.origin.y, r.size.width, r.size.height);

    user_window_t* window = window_with_id(event->window_id);
    if (!window) {
        printf("No window found with the specified ID\n");
        return;
    }

    if (window->is_minimized) {
        printf("Window was already minimized, doing nothing\n");
        return;
    }

    // Immediately stop doing extra work for the window
    window->is_minimized = true;
    window->unminimized_frame = window->frame;
    if (window->unminimized_snapshot) {
        layer_teardown(window->unminimized_snapshot);
        window->unminimized_snapshot = NULL;
    }

	window->unminimized_snapshot = layer_snapshot(
		window->layer, 
		rect_make(
			point_zero(),
            window->unminimized_frame.size
		)
	);

    /*
    // Darken the preview
	for (int row = 0; row < s.height; row++) {
        uint32_t row_start = row * (s.width * gfx_bytes_per_pixel());
        for (int col = 0; col < s.width; col++) {
		//memcpy(snapshot_row, row_start, frame.size.width * gfx_bytes_per_pixel());
            for (int b = 0; b < gfx_bytes_per_pixel(); b++) {
                window->unminimized_snapshot->raw[row_start + (col * gfx_bytes_per_pixel()) + b] *= 0.75;
            }
        }
    }
    window->unminimized_snapshot->alpha = 0.5;
    */

    Rect task_view_frame = event->task_view_frame;
	Size screen_size = screen_resolution();
    Rect dest_frame = rect_make(
        point_make(
            rect_min_x(task_view_frame),
            screen_size.height
        ),
        task_view_frame.size
    );
    awm_animation_minimize_window_t* anim = awm_animation_minimize_window_init(300, window, dest_frame, window->frame);
    awm_animation_start(anim);

    // Allow mouse control to pass to the window behind the minimized one
    mouse_recompute_status();
}

void window_unminimize_from_message(awm_dock_task_view_clicked_event_t* event) {
    printf("Unminimizing window with ID %d\n", event->window_id);

    user_window_t* window = window_with_id(event->window_id);
    if (!window) {
        printf("No window found with the specified ID\n");
        return;
    }

    if (!window->is_minimized) {
        printf("Window wasn't already minimized, doing nothing\n");
        return;
    }

    window->is_minimized = false;
    if (window->unminimized_snapshot) {
        layer_teardown(window->unminimized_snapshot);
        window->unminimized_snapshot = NULL;
    }

    // Stop displaying the hover preview once we begin unminimizing
    minimized_preview_clear();

    // Unminimized windows are always placed on top
    window_move_to_top(window);

    // TODO(PT): Need to call mouse_recompute_status here, new animation?
    awm_animation_unminimize_window_t* anim = awm_animation_unminimize_window_init(300, window, window->unminimized_frame, window->frame);
    awm_animation_start(anim);
}

void window_handle_left_click(user_window_t* window,  Point mouse_within_window) {
    if (!window) {
        printf("window_handle_left_click got NULL window, ignoring\n");
        return;
    }

    // Is the mouse within the window's content view?
    if (rect_contains_point(window->content_view->frame, mouse_within_window)) {
        Point mouse_within_content_view = point_make(
            mouse_within_window.x - rect_min_x(window->content_view->frame),
            mouse_within_window.y - rect_min_y(window->content_view->frame)
        );
        awm_mouse_left_click_msg_t msg = {0};
        msg.event = AWM_MOUSE_LEFT_CLICK;
        msg.click_point = mouse_within_content_view;
        amc_message_send(window->owner_service, &msg, sizeof(msg));
    }
}

void window_handle_left_click_ended(user_window_t* window,  Point mouse_within_window) {
    if (!window) {
        printf("window_handle_left_click_ended got NULL window, ignoring\n");
        return;
    }

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
    if (!window) {
        printf("window_handle_mouse_entered got NULL window, ignoring\n");
        return;
    }

	// Inform the window the mouse has just entered it
	amc_msg_u32_1__send(window->owner_service, AWM_MOUSE_ENTERED);
}

void window_handle_mouse_exited(user_window_t* window) {
    if (!window) {
        printf("window_handle_mouse_exited got NULL window, ignoring\n");
        return;
    }

	// Inform the window the mouse has just exited it
	amc_msg_u32_1__send(window->owner_service, AWM_MOUSE_EXITED);

    // Ensure we disable the title bar buttons
    window_redraw_title_bar(window, false, false, false);
}

void window_handle_mouse_moved(user_window_t* window, Point mouse_within_window) {
    if (!window) {
        printf("window_handle_mouse_moved got NULL window, ignoring\n");
        return;
    }

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
    if (!window) {
        printf("window_handle_keyboard_event got NULL window, ignoring\n");
        return;
    }

    amc_msg_u32_2__send(window->owner_service, event, key);
}

static void _window_fetch_framebuf(user_window_t* window) {
    if (!window) {
        printf("_window_fetch_framebuf got NULL window, ignoring\n");
        return;
    }

    if (window->remote_process_died) {
        printf("Skipping framebuf fetch for window because the remote process is dead: %s\n", window->owner_service);
        return;
    }
    if (window->is_minimized) {
        //printf("Skipping framebuf fetch for window because it is minimized: %s\n", window->owner_service);
        return;
    }

	window->has_done_first_draw = true;
	blit_layer(
		window->layer, 
		window->content_view->layer, 
		window->content_view->frame, 
		rect_make(point_zero(), window->content_view->frame.size)
	);
}

void window_queue_fetch(user_window_t* window) {
    if (!window) {
        printf("window_queue_fetch got NULL window, ignoring\n");
        return;
    }

    if (array_index(windows_to_fetch_this_cycle, window) == -1) {
        //printf("Ready for redraw: %s\n", window->owner_service);
        array_insert(windows_to_fetch_this_cycle, window);
    }
    desktop_view_queue_composite((view_t*)window);
}

void desktop_view_queue_composite(view_t* view) {
    if (array_index(views_to_composite_this_cycle, view) == -1) {
        array_insert(views_to_composite_this_cycle, view);
    }
}

void desktop_view_queue_extra_draw(view_t* view, Rect extra) {
    rect_add(view->extra_draws_this_cycle, extra);
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

void window_redraw_title_bar(user_window_t* window, bool title_bar_active, bool close_button_active, bool minimize_button_active) {
    assert(window != NULL, "window_redraw_title_bar() got NULL window");
    if (!window->has_title_bar) {
        // No work to do 
        return;
    }

	if (!_g_title_bar_image) {
		//printf("No images yet...\n");
		return;
	}

	Size title_bar_size = size_make(window->frame.size.width, WINDOW_TITLE_BAR_HEIGHT);
    Rect title_bar_frame = rect_make(
        point_zero(), 
        size_make(title_bar_size.width, WINDOW_TITLE_BAR_VISIBLE_HEIGHT)
    );
    //printf("rendering with frame %d\n", title_bar_size.width);
	image_render_to_layer(
		_g_title_bar_image, 
		window->layer, 
        title_bar_frame
	);

    // Draw a highlight border if necessary
    if (title_bar_active) {
        draw_rect(
            window->layer,
            title_bar_frame,
            // PT: Matches the shade drawn by libgui
            color_make(200, 200, 200),
            1
        );
    }

	image_t* x_image = (close_button_active) ? _g_title_bar_x_filled : _g_title_bar_x_unfilled;
	uint32_t icon_height = x_image->size.height;
	window->close_button_frame = rect_make(
		point_make(icon_height * 1.5, 5), 
		x_image->size
	);
	image_render_to_layer(
		x_image, 
		window->layer, 
		window->close_button_frame
	);

	window->minimize_button_frame = rect_make(
		point_make(icon_height * 2.75, 5), 
		x_image->size
	);
	image_t* minimize_image = (close_button_active) ? _g_title_bar_minimize_filled : _g_title_bar_minimize_unfilled;
	image_render_to_layer(
		minimize_image, 
		window->layer, 
		window->minimize_button_frame
	);

	// Draw window title
	_write_window_title(window);

    // Push the update
    // TODO(PT): Only queue the button frames, and only do so if there was a state change
    compositor_queue_rect_to_redraw(rect_make(window->frame.origin, title_bar_size));
}

void windows_fetch_queued_windows(void) {
    for (int32_t i = 0; i < windows_to_fetch_this_cycle->size; i++) {
        user_window_t* window_to_update = array_lookup(windows_to_fetch_this_cycle, i);
        _window_fetch_framebuf(window_to_update);
    }
}

void desktop_views_flush_queues(void) {
    for (int32_t i = windows_to_fetch_this_cycle->size - 1; i >= 0; i--) {
        array_remove(windows_to_fetch_this_cycle, i);
    }
    for (int32_t i = views_to_composite_this_cycle->size - 1; i >= 0; i--) {
        array_remove(views_to_composite_this_cycle, i);
    }
}

void windows_init(void) {
    windows = array_create(MAX_WINDOW_COUNT);
    windows_with_z_order = array_create(MAX_WINDOW_COUNT);
    windows_without_z_order = array_create(MAX_WINDOW_COUNT);
    desktop_views = array_create(MAX_WINDOW_COUNT);
    windows_to_fetch_this_cycle = array_create(MAX_WINDOW_COUNT);
    views_to_composite_this_cycle = array_create(MAX_WINDOW_COUNT);

    // Set up the desktop shortcut grid
    _g_desktop_shortcuts_state.grid_slots = array_create(256);

    Size grid_slot_size = desktop_shortcut_grid_slot_size();
    Size screen_size = screen_resolution();
    printf("Screen resolution: %d %d\n", screen_size.width, screen_size.height);
    // Iterate columnly so when searching for a free space linearly we fill in columns first
    for (int32_t x = 0; x < (screen_size.width - grid_slot_size.width); x += grid_slot_size.width) {
        for (int32_t y = 0; y < screen_size.height - grid_slot_size.height; y += grid_slot_size.height) {
            desktop_shortcut_grid_slot_t* slot = calloc(1, sizeof(desktop_shortcut_grid_slot_t));
            slot->frame = rect_make(
                point_make(x, y),
                grid_slot_size
            );
            array_insert(_g_desktop_shortcuts_state.grid_slots, slot);
        }
    }
    // We now know the maximum possible number of desktop shortcuts,
    // based on the current screen resolution
    _g_desktop_shortcuts_state.shortcuts = array_create(_g_desktop_shortcuts_state.grid_slots->size);
}

array_t* desktop_shortcuts(void) {
    return _g_desktop_shortcuts_state.shortcuts;
}

uint32_t rect_shared_area(Rect a, Rect b) {
    uint32_t dx = min(rect_max_x(a), rect_max_x(b)) - max(rect_min_x(a), rect_min_x(b));
    uint32_t dy = min(rect_max_y(a), rect_max_y(b)) - max(rect_min_y(a), rect_min_y(b));
    if (dx >= 0 && dy >= 0) {
        return dx * dy;
    }
    return 0;
}

desktop_shortcut_grid_slot_t* desktop_shortcut_grid_slot_for_rect(Rect r) {
    uint32_t greatest_shared_area = 0;
    desktop_shortcut_grid_slot_t* best_match = NULL;
    for (int32_t i = 0; i < _g_desktop_shortcuts_state.grid_slots->size; i++) {
        desktop_shortcut_grid_slot_t* slot = array_lookup(_g_desktop_shortcuts_state.grid_slots, i);
        if (slot->occupant != NULL) {
            continue;
        }

        // If the right and bottom corners are in this grid slot, we know we can stop here
        // There's no way another slot further along could contain more of the shortcut.
        // Right edge bounded?
        bool can_stop_here = false;
        if (rect_max_x(r) >= rect_min_x(slot->frame) && rect_max_x(r) < rect_max_x(slot->frame)) {
            // Bottom edge bounded?
            if (rect_max_y(r) >= rect_min_y(slot->frame) && rect_max_y(r) < rect_max_y(slot->frame)) {
                can_stop_here = true;
            }
        }

        if (!rect_intersects(r, slot->frame)) {
            continue;
        }
        uint32_t shared_area = rect_shared_area(r, slot->frame);
        if (shared_area > greatest_shared_area) {
            greatest_shared_area = shared_area;
            best_match = slot;
        }

        if (can_stop_here) {
            break;
        }
    }
    return best_match;
}

void desktop_shortcut_handle_mouse_exited(desktop_shortcut_t* shortcut) {
    shortcut->in_soft_click = false;
    shortcut->in_mouse_hover = false;
    desktop_shortcut_render(shortcut);
}

void desktop_shortcut_handle_mouse_entered(desktop_shortcut_t* shortcut) {
    shortcut->in_soft_click = false;
    shortcut->in_mouse_hover = true;
    desktop_shortcut_render(shortcut);
}

void desktop_shortcut_highlight(desktop_shortcut_t* shortcut) {
    shortcut->in_soft_click = true;
    shortcut->first_click_start_time = ms_since_boot();
    desktop_shortcut_render(shortcut);
}

void desktop_shortcut_unhighlight(desktop_shortcut_t* shortcut) {
    shortcut->in_soft_click = false;
    desktop_shortcut_render(shortcut);
}

void desktop_views_add(view_t* view) {
    array_insert(desktop_views, view);
    desktop_view_queue_composite(view);
    windows_invalidate_drawable_regions_in_rect(view->frame);
}

void desktop_view_remove_and_teardown(view_t** view_ptr) {
    view_t* view = *view_ptr;
    int32_t index = array_index(desktop_views, view);
    assert(index >= 0, "Invalid index");
    array_remove(desktop_views, index);

    // May be queued to render this frame
    index = array_index(views_to_composite_this_cycle, view);
    if (index >= 0) {
        array_remove(views_to_composite_this_cycle, index);
    }
    
    //desktop_views_ready_to_composite_arra
    windows_invalidate_drawable_regions_in_rect(view->frame);
	compositor_queue_rect_to_redraw(view->frame);
    view_destroy(view_ptr);
}

array_t* all_desktop_views(void) {
    array_t* out = array_create(windows->size + desktop_views->size + 1);
    // Windows are always highest in the view hierarchy

    //array_t* windows_in_z_order = array_create(windows_with_z_order->size + windows_without_z_order->size);
    // Always draw windows without Z-order on top
    for (int i = 0; i < windows_without_z_order->size; i++) {
        array_insert(out, array_lookup(windows_without_z_order, i));
    }
    for (int i = 0; i < windows_with_z_order->size; i++) {
        array_insert(out, array_lookup(windows_with_z_order, i));
    }

    /*
    for (int32_t i = 0; i < windows->size; i++) {
        array_insert(out, array_lookup(windows, i));
    }
    */
    for (int32_t i = 0; i < desktop_views->size; i++) {
        array_insert(out, array_lookup(desktop_views, i));
    }
    return out;
}

static Size _desktop_shortcut_size(void) {
    return size_make(100, 65);
}

Size desktop_shortcut_grid_slot_size(void) {
    Size shortcut_size = _desktop_shortcut_size();
    return size_make(
        shortcut_size.width + 16,
        shortcut_size.height + 30
    );
}

void desktop_shortcut_render(desktop_shortcut_t* ds) {
    desktop_shortcuts_state_t* shortcuts_state = &_g_desktop_shortcuts_state;
    Size shortcut_icon_size = _desktop_shortcut_size();
    Size icon_image_size = size_make(
        shortcut_icon_size.width - 16,
        shortcut_icon_size.height - 16
    );

    uint32_t icon_x_margin = shortcut_icon_size.width - icon_image_size.width;
    uint32_t icon_y_margin = 3;
    uint32_t label_height = shortcut_icon_size.height - icon_image_size.height;

    if (ds->in_soft_click) {
        //Color background_color = ds->in_soft_click ? color_make(127, 127, 255) : color_white();
        draw_rect(
            ds->view->layer, 
            rect_make(point_zero(), shortcut_icon_size), 
            color_make(127, 127, 255), 
            THICKNESS_FILLED
        );
    }
    else {
        blit_layer(
            ds->view->layer,
            desktop_background_layer(),
            rect_make(point_zero(), ds->view->frame.size),
            ds->view->frame
        );
    }

    if (_g_executable_image) {
        image_render_to_layer(
            _g_executable_image,
            ds->view->layer,
            rect_make(
                point_make(
                    icon_x_margin,
                    icon_y_margin
                ),
                size_make(
                    shortcut_icon_size.width - (icon_x_margin * 2),
                    icon_image_size.height - (icon_y_margin * 2)
                )
            )
        );
    }

    Size view_size = ds->view->frame.size;
	Point mid = point_make(
        view_size.width / 2.0,
        view_size.height - (label_height / 2.0)
    );
	Size font_size = size_make(8, 10);
	uint32_t len = strlen(ds->display_name);
	Point label_origin = point_make(
		mid.x - ((font_size.width * len) / 2.0),
		mid.y - (font_size.height / 2.0)
	);

    Color text_color = color_make(50, 50, 50);
    // If the background gradient is too dark, set the shortcuts text color to white so it's always visible.
    // Per ITU-R BT.709
    Color outer_bg = background_gradient_outer_color();
    double luma = (0.2126 * outer_bg.val[0]) + (0.7152 * outer_bg.val[1]) + (0.0722 * outer_bg.val[2]);
    //printf("Luma: %.2f\n", luma);
    if (luma < 64) {
        text_color = color_make(205, 205, 205);
    }
    if (ds->in_soft_click) {
        text_color = color_white();
    }

	for (uint32_t i = 0; i < len; i++) {
		draw_char(
			ds->view->layer,
			ds->display_name[i],
			label_origin.x + (font_size.width * i),
			label_origin.y,
			text_color,
			font_size
		);
	}

    if (ds->in_mouse_hover || ds->in_soft_click) {
        Color outline_color = ds->in_mouse_hover ? color_dark_gray() : color_blue();
        draw_rect(
            ds->view->layer,
            rect_make(
                point_zero(),
                ds->view->frame.size
            ),
            outline_color,
            2
        );
    }

    desktop_view_queue_composite(ds->view);
}

Rect desktop_shortcut_frame_within_grid_slot(desktop_shortcut_grid_slot_t* grid_slot) {
    Size shortcut_icon_size = _desktop_shortcut_size();
    return rect_make(
        point_make(
            rect_mid_x(grid_slot->frame) - (shortcut_icon_size.width / 2.0),
            rect_mid_y(grid_slot->frame) - (shortcut_icon_size.height / 2.0)
        ),
        shortcut_icon_size
    );
}

Rect desktop_shortcut_place_in_grid_slot(desktop_shortcut_t* shortcut, desktop_shortcut_grid_slot_t* grid_slot) {
    // Clear the previous grid slot
    if (shortcut->grid_slot != NULL) {
        shortcut->grid_slot->occupant = NULL;
    }

    Size shortcut_icon_size = _desktop_shortcut_size();
    shortcut->view->frame.origin = point_make(
        rect_mid_x(grid_slot->frame) - (shortcut_icon_size.width / 2.0),
        rect_mid_y(grid_slot->frame) - (shortcut_icon_size.height / 2.0)
    );
    grid_slot->occupant = shortcut;
    shortcut->grid_slot = grid_slot;
    return shortcut->view->frame;
}

static void desktop_shortcuts_add_and_place_in_slot(
    const char* display_name, 
    const char* program_path, 
    desktop_shortcut_grid_slot_t* grid_slot
) {
    desktop_shortcuts_state_t* shortcuts_state = &_g_desktop_shortcuts_state;
    desktop_shortcut_t* shortcut = calloc(1, sizeof(desktop_shortcut_t));

    shortcut->view = view_create(rect_make(point_zero(), _desktop_shortcut_size()));
    shortcut->program_path = program_path;
    shortcut->display_name = display_name;
    array_insert(shortcuts_state->shortcuts, shortcut);

    desktop_shortcut_place_in_grid_slot(shortcut, grid_slot);

    desktop_views_add(shortcut->view);

    desktop_shortcut_render(shortcut);
}

static void desktop_shortcuts_add(const char* display_name, const char* program_path) {
    // Find the next empty grid slot
    desktop_shortcuts_state_t* shortcuts_state = &_g_desktop_shortcuts_state;
    desktop_shortcut_grid_slot_t* found_slot = NULL;
    for (int32_t i = 0; i < shortcuts_state->grid_slots->size; i++) {
        desktop_shortcut_grid_slot_t* slot = array_lookup(shortcuts_state->grid_slots, i);
        if (!slot->occupant) {
            found_slot = slot;
            break;
        }
    }
    //assert(found_slot != NULL, "No more room to add another desktop shortcut");
    desktop_shortcuts_add_and_place_in_slot(display_name, program_path, found_slot);
}

static void desktop_shortcuts_add_and_place_in_slot_by_coordinate(
    const char* display_name, 
    const char* program_path, 
    int x, 
    int y
) {
    desktop_shortcuts_state_t* shortcuts_state = &_g_desktop_shortcuts_state;

    Size grid_slot_size = desktop_shortcut_grid_slot_size();
    Size screen_size = screen_resolution();

    int rows = screen_size.height / grid_slot_size.height;
    int cols = screen_size.width / grid_slot_size.width;
    assert(x < cols, "X is more than the number of possible columns");
    assert(y < rows, "Y is more than the number of possible rows");
    // By definition
    int shortcut_slots_per_col = rows;
    int idx = (x * shortcut_slots_per_col) + y;
    desktop_shortcut_grid_slot_t* slot = array_lookup(shortcuts_state->grid_slots, idx);
    desktop_shortcuts_add_and_place_in_slot(display_name, program_path, slot);
}

void windows_fetch_resource_images(void) {
	_g_title_bar_image = load_image("/images/titlebar7.bmp");
	_g_title_bar_x_filled = load_image("/images/titlebar_x_filled2.bmp");
	_g_title_bar_x_unfilled = load_image("/images/titlebar_x_unfilled2.bmp");
	_g_title_bar_minimize_filled = load_image("/images/titlebar_minimize_filled.bmp");
	_g_title_bar_minimize_filled = load_image("/images/titlebar_minimize_filled.bmp");
	_g_title_bar_minimize_unfilled = load_image("/images/titlebar_minimize_unfilled.bmp");
    for (int32_t i = 0; i < windows->size; i++) {
        user_window_t* w = array_lookup(windows, i);
        window_redraw_title_bar(w, false, false, false);
    }

    _g_executable_image = load_image("/images/executable_icon.bmp");

    desktop_shortcuts_add("Preferences", "/usr/applications/preferences");
    desktop_shortcuts_add("Logs Viewer", "/usr/applications/logs_viewer");
    desktop_shortcuts_add("DOOM", "/usr/applications/doom");
    desktop_shortcuts_add("Breakout", "/usr/applications/breakout");
    desktop_shortcuts_add("2048", "/usr/applications/2048");
    desktop_shortcuts_add_and_place_in_slot_by_coordinate("Notepad", "/usr/applications/textpad", 1, 0);
    desktop_shortcuts_add_and_place_in_slot_by_coordinate("Exploit", "/magic/exploit", 1, 1);
    desktop_shortcuts_add_and_place_in_slot_by_coordinate("VAS Viz", "/usr/applications/task_viewer", 1, 2);
    desktop_shortcuts_add_and_place_in_slot_by_coordinate("GameBoy", "/usr/applications/gb_emu", 1, 3);
    desktop_shortcuts_add_and_place_in_slot_by_coordinate("File Browser", "/usr/applications/file_browser", 1, 4);
}

static int _g_next_window_id = 0;

user_window_t* window_create(const char* owner_service, uint32_t width, uint32_t height) {
    // Ask the kernel to inform us when this process dies
    amc_notify_when_service_dies_cmd_t req = {0};
    req.event = AMC_REGISTER_NOTIFICATION_SERVICE_DIED;
    snprintf(&req.remote_service, sizeof(req.remote_service), owner_service);
    amc_message_send(AXLE_CORE_SERVICE_NAME, &req, sizeof(req));

	user_window_t* window = calloc(1, sizeof(user_window_t));
	array_insert(windows, window);

    // Don't place the dock in the Z-order hierarchy
    if (!amc_service_is_awm_dock(owner_service)) {
        array_insert(windows_with_z_order, window);
    }
    else {
        array_insert(windows_without_z_order, window);
    }

	window->drawable_rects = array_create(256);
	window->extra_draws_this_cycle = array_create(512);

	// Shared layer is size of the screen to allow window resizing
    Size res = screen_resolution();
	uint32_t shmem_size = res.width * res.height * screen_bytes_per_pixel(); 

    amc_shared_memory_create_cmd_t cmd = {
        .event = AMC_SHARED_MEMORY_CREATE_REQUEST,
        .buffer_size = shmem_size
    };
    snprintf(cmd.remote_service_name, sizeof(cmd.remote_service_name), "%s", owner_service);
    
    // Ask the kernel to set up a shared memory mapping we'll use for the framebuffer
    amc_message_send(AXLE_CORE_SERVICE_NAME,  &cmd, sizeof(cmd));
	amc_message_t* msg;
    amc_message_await__u32_event(AXLE_CORE_SERVICE_NAME, AMC_SHARED_MEMORY_CREATE_RESPONSE, &msg);

    amc_shared_memory_create_response_t* shmem_info = (amc_shared_memory_create_response_t*)&msg->body;
    uintptr_t shmem_local = shmem_info->local_buffer_start;
    uintptr_t shmem_remote = shmem_info->remote_buffer_start;

    // Another message has been delivered, so the owner_service pointer is no longer valid
    // Re-set it to a copy we made so it can be safely used down below
    owner_service = &cmd.remote_service_name;
    printf("owner_service 0x%p cmd.remote_service_name 0x%p\n", owner_service, cmd.remote_service_name);

	// Place the window in the center of the screen, minus the dock's height
	Point origin = point_make(
		(res.width / 2) - (width / 2),
		((res.height - AWM_DOCK_HEIGHT) / 2) - (height / 2)
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
	//window_redraw_title_bar(window, false, false, false);

	// Make the new window show up on top
	window_move_to_top(window);

	// Now that we've configured the initial window state on our end, 
	// provide the buffer to the client
	printf("AWM made shared framebuffer for %s\n", owner_service);
	printf("\tAWM    memory: %p - %p\n", shmem_local, shmem_local + shmem_size);
	printf("\tRemote memory: %p - %p\n", shmem_remote, shmem_remote + shmem_size);

    Size screen_size = screen_resolution();
    awm_create_window_response_t req2 = {
        .event = AWM_CREATE_WINDOW_RESPONSE,
        .screen_resolution = screen_size,
        .bytes_per_pixel = screen_bytes_per_pixel(),
        .framebuffer = shmem_remote
    };
    amc_message_send(owner_service, &req2, sizeof(req2));

    awm_animation_open_window_t* open_window_animation = NULL;
    if (amc_service_is_awm_dock(owner_service)) {
        int dock_height = AWM_DOCK_HEIGHT;
        Rect initial_frame = rect_make(
            point_make(0, screen_size.height),
            size_make(screen_size.width, dock_height)
        );
        Rect final_frame = rect_make(
            point_make(0, screen_size.height - dock_height),
            size_make(screen_size.width, dock_height)
        );

        open_window_animation = awm_animation_open_window_init_ex(200, window, final_frame, initial_frame);
        window->has_title_bar = false;
        window->is_movable = false;
        window->is_resizable = false;
    }
    else {
        // Make the window a bit bigger than the user requested to accomodate for decorations
        int full_window_width = width + (WINDOW_BORDER_MARGIN * 2);
        Size title_bar_size = size_make(full_window_width, WINDOW_TITLE_BAR_HEIGHT);
        Size full_window_size = size_make(
            full_window_width, 
            // We only need to add the border margin on the bottom edge
            // The top edge does not have a border margin
            height + title_bar_size.height + WINDOW_BORDER_MARGIN
        );

        open_window_animation = awm_animation_open_window_init(200, window, rect_make(origin, full_window_size));
        window->has_title_bar = true;
        window->is_movable = true;
        window->is_resizable = true;
    }

    awm_animation_start(open_window_animation);

    // Window IDs increase monotonically
    window->window_id = _g_next_window_id;
    _g_next_window_id += 1;

    // If this is a window other than the dock, inform the dock
    if (!amc_service_is_awm_dock(owner_service)) {
        awm_dock_window_created_event_t msg = {
            .event = AWM_DOCK_WINDOW_CREATED,
            .window_id = window->window_id,
            .title_len = strlen(window->title),
            .title = 0,
        };
        snprintf(msg.title, sizeof(msg.title), "%s", window->title);
        amc_message_send("com.axle.awm_dock", &msg, sizeof(msg));
    }

    return window;
}

void window_destroy(user_window_t* window) {
    assert(window != NULL, "Expected non-NULL window");

    int32_t i = array_index(windows, window);
    assert(i >= 0, "Window not found");
    array_remove(windows, i);

    i = array_index(windows_with_z_order, window);
    if (i >= 0) {
        array_remove(windows_with_z_order, i);
    }
    i = array_index(windows_without_z_order, window);
    if (i >= 0) {
        array_remove(windows_without_z_order, i);
    }
    i = array_index(windows_to_fetch_this_cycle, window);
    if (i >= 0) {
        array_remove(windows_to_fetch_this_cycle, i);
    }
    i = array_index(views_to_composite_this_cycle, window);
    if (i >= 0) {
        array_remove(views_to_composite_this_cycle, i);
    }

    // The windows behind this one need to have their visible areas recomputed
	windows_invalidate_drawable_regions_in_rect(window->frame);
	compositor_queue_rect_to_redraw(window->frame);

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

array_t* desktop_views_ready_to_composite_array(void) {
    return views_to_composite_this_cycle;
}

void windows_invalidate_drawable_regions_in_rect(Rect r) {
    //printf("windows_invalidate_drawable_regions_in_rect(%d, %d, %d, %d)\n", r.origin.x, r.origin.y, r.size.width, r.size.height);
    array_t* all_views = all_desktop_views();
    for (int32_t i = all_views->size - 1; i >= 0; i--) {
        view_t* view = array_lookup(all_views, i);
        if (!rect_intersects(view->frame, r)) {
            continue;
        }

        // Recompute the non-occluded rects belonging to the view
        for (int32_t j = view->drawable_rects->size - 1; j >= 0; j--) {
            Rect* r = array_lookup(view->drawable_rects, j);
            free(r);
            array_remove(view->drawable_rects, j);
        }

        rect_add(view->drawable_rects, view->frame);

        for (int32_t j = i - 1; j >= 0; j--) {
            view_t* occluding_view = array_lookup(all_views, j);
            if (!rect_intersects(occluding_view->frame, view->frame)) {
                continue;
            }
            view->drawable_rects = update_occlusions(view->drawable_rects, occluding_view->frame);
            if (!view->drawable_rects->size) {
                break;
            }
        }
        //printf("\tUpdated drawable rects of view %d to contain %d rects\n", i, view->drawable_rects->size);
        if (view->drawable_rects->size) {
            /*
            for (int j = 0; j < view->drawable_rects->size; j++) {
                Rect* r = array_lookup(view->drawable_rects, j);
                printf("\t\t(%d, %d, %d, %d)\n", rect_min_x(*r), rect_min_y(*r), r->size.width, r->size.height);
            }
            */
            desktop_view_queue_composite(view);
        }
        else {
            //printf("Will not composite %s because it's fully occluded\n", window->owner_service);
        }
    }
    array_destroy(all_views);
}

void windows_composite(ca_layer* dest, Rect updated_rect) {
    // TODO(PT): This is unused?!
    //printf("Update rect: (%d, %d), (%d, %d)\n", rect_min_x(updated_rect), rect_min_y(updated_rect), updated_rect.size.width, updated_rect.size.height);

    for (uint32_t i = 0; i < windows->size; i++) {
        user_window_t* window = array_lookup(windows, i);
        Rect* r = calloc(1, sizeof(Rect));
        *r = window->frame;
        array_insert(window->drawable_rects, r);
    }
    
    printf("Windows without z order %d with %d\n", windows_without_z_order->size, windows_with_z_order->size);
    array_t* windows_in_z_order = array_create(windows_with_z_order->size + windows_without_z_order->size);
    // Always draw windows without Z-order on top
    for (int i = 0; i < windows_without_z_order->size; i++) {
        array_insert(windows_in_z_order, array_lookup(windows_without_z_order, i));
    }
    for (int i = 0; i < windows_with_z_order->size; i++) {
        array_insert(windows_in_z_order, array_lookup(windows_with_z_order, i));
    }

    for (int i = 0; i < windows_in_z_order->size; i++) {
        user_window_t* window = array_lookup(windows_in_z_order, i);
        if (!window->has_done_first_draw) {
            continue;
        }

        for (int outer_rect_idx = 0; outer_rect_idx < window->drawable_rects->size; outer_rect_idx++) {
            Rect* outer_rect_ptr = array_lookup(window->drawable_rects, outer_rect_idx);
            Rect outer_rect = *outer_rect_ptr;

            for (int j = i + 1; j < windows_in_z_order->size; j++) {
                user_window_t* bg_window = array_lookup(windows_in_z_order, j);
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

        if (window->has_title_bar) {
            // TODO(PT): Can these calls be combined?
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
        else {
            blit_layer(
                dest, 
                window->layer, 
                window->frame, 
                rect_make(
                    point_zero(), 
                    window->frame.size
                )
            );
        }
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

    for (uint32_t i = 0; i < windows_in_z_order->size; i++) {
        user_window_t* window = array_lookup(windows_in_z_order, i);
        for (int32_t j = window->drawable_rects->size - 1; j >= 0; j--) {
            Rect* r = array_lookup(window->drawable_rects, j);
            free(r);
            array_remove(window->drawable_rects, j);
        }
    }

    array_destroy(windows_in_z_order);
}

view_t* view_create(Rect frame) {
    view_t* v = calloc(1, sizeof(view_t));
    v->frame = frame;
	v->layer = create_layer(frame.size);
    v->drawable_rects = array_create(256);
    v->extra_draws_this_cycle = array_create(256);
    return v;
}

void view_destroy(view_t** view_ptr) {
    view_t* view = *view_ptr;
    array_destroy(view->drawable_rects);
    array_destroy(view->extra_draws_this_cycle);
    layer_teardown(view->layer);
    free(view);
    // Prevent UAF
    *view_ptr = NULL;
}

void draw_queued_extra_draws(array_t* views, ca_layer* dest_layer) {
    for (int32_t i = 0; i < views->size; i++) {
        //user_window_t* window = array_lookup(windows, i);
        view_t* view = array_lookup(views, i);
        for (int32_t j = view->extra_draws_this_cycle->size - 1; j >= 0; j--) {
            Rect* r_ptr = array_lookup(view->extra_draws_this_cycle, j);
            Rect r = *r_ptr;
            int32_t offset_x = r.origin.x - rect_min_x(view->frame);
            int32_t offset_y = r.origin.y - rect_min_y(view->frame);
            if (rect_max_x(r) < rect_min_x(view->frame) || rect_max_y(r) < rect_min_y(view->frame)) {
                continue;
            }

            int x_origin = rect_min_x(view->frame) - rect_min_x(r);
            if (x_origin > 0){ 
                r.origin.x += x_origin;
            }
            int y_origin = rect_min_y(view->frame) - rect_min_y(r);
            if (y_origin > 0){ 
                r.origin.y += y_origin;
            }

            int x_overhang = rect_max_x(r) - rect_max_x(view->frame);
            if (x_overhang > 0) {
                r.size.width -= x_overhang;
            }
            int y_overhang = rect_max_y(r) - rect_max_y(view->frame);
            if (y_overhang > 0) {
                r.size.height -= y_overhang;
            }

            blit_layer(
                dest_layer,
                view->layer,
                r,
                rect_make(
                    point_make(offset_x, offset_y), 
                    r.size
                )
            );
            //draw_rect(_screen.vmem, r, color_rand(), 1);
        }
    }
}

void complete_queued_extra_draws(array_t* views, ca_layer* source_layer, ca_layer* dest_layer) {
    for (int32_t i = 0; i < views->size; i++) {
        view_t* view = array_lookup(views, i);
        for (int32_t j = view->extra_draws_this_cycle->size - 1; j >= 0; j--) {
            Rect* r_ptr = array_lookup(view->extra_draws_this_cycle, j);
            Rect r = *r_ptr;
            blit_layer(dest_layer, source_layer, r, r);
            array_remove(view->extra_draws_this_cycle, j);
            free(r_ptr);
        }
    }
}

void draw_views_to_layer(array_t* views, ca_layer* dest_layer) {
    for (int32_t i = 0; i < views->size; i++) {
        view_t* view = array_lookup(views, i);
        for (int32_t j = 0; j < view->drawable_rects->size; j++) {
            Rect* r_ptr = array_lookup(view->drawable_rects, j);
            Rect r = *r_ptr;
            uint32_t offset_x = r.origin.x - rect_min_x(view->frame);
            uint32_t offset_y = r.origin.y - rect_min_y(view->frame);
            blit_layer(
                dest_layer,
                view->layer,
                r,
                rect_make(
                    point_make(offset_x, offset_y), 
                    r.size
                )
            );
            //draw_rect(_screen.vmem, r, color_rand(), 1);
        }
    }
}

void minimized_preview_display_for_window(user_window_t* window, Rect task_view_frame) {
    // If we were previously displaying a different preview, clean it up
    if (_g_minimized_window_preview != NULL) {
        desktop_view_remove_and_teardown(&_g_minimized_window_preview);
    }

    Rect snapshot_frame = window->unminimized_frame;
	// Maintain aspect ratio of the window
	int max_preview_length = 80;
	double aspect_ratio = snapshot_frame.size.height / (double)snapshot_frame.size.width;
	Size preview_size = size_make(max_preview_length, max_preview_length * aspect_ratio);
	if (snapshot_frame.size.width > snapshot_frame.size.height) {
		aspect_ratio = snapshot_frame.size.width / (double)snapshot_frame.size.height;
		preview_size = size_make(max_preview_length * aspect_ratio, max_preview_length);
	}

    Size screen_size = screen_resolution();
    int task_view_midpoint_x = rect_mid_x(task_view_frame);
    
    int preview_origin_x = task_view_midpoint_x - (preview_size.width / 2);
    preview_origin_x = max(preview_origin_x, 20);
    preview_origin_x = min(preview_origin_x, screen_size.width - preview_size.width - 20);

	_g_minimized_window_preview = view_create(
		rect_make(
			point_make(
                preview_origin_x, 
                screen_size.height - preview_size.height - (AWM_DOCK_HEIGHT * 1.2)
            ),
			preview_size
		)
	);

	blit_layer_scaled(
		_g_minimized_window_preview->layer,
		window->unminimized_snapshot,
		_g_minimized_window_preview->frame.size
	);

    int border_width = 3;
    draw_rect(
        _g_minimized_window_preview->layer,
        rect_make(
            point_zero(), 
            size_make(
                window->unminimized_frame.size.width - border_width, 
                window->unminimized_frame.size.height - border_width
            )
        ),
        color_white(),
        border_width
    );

    desktop_views_add(_g_minimized_window_preview);
}

void minimized_preview_clear(void) {
    if (_g_minimized_window_preview != NULL) {
        desktop_view_remove_and_teardown(&_g_minimized_window_preview);
    }
}
