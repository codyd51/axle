#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>

#include <agx/font/font.h>
#include <agx/lib/shapes.h>
#include <agx/lib/ca_layer.h>
#include <agx/lib/screen.h>

#include <libamc/libamc.h>

#include <stdlibadd/assert.h>
#include <stdlibadd/array.h>

#include <preferences/preferences_messages.h>
#include <kb_driver/kb_driver_messages.h>

#include "awm.h"
#include "math.h"
#include "window.h"
#include "awm_messages.h"
#include "utils.h"
#include "effects.h"
#include "awm_internal.h"
#include "animations.h"
#include "composite.h"

typedef struct incremental_mouse_state {
	int8_t state;
	int32_t rel_x;
	int32_t rel_y;
	int32_t rel_z;
	uint32_t combined_msg_count;
} incremental_mouse_state_t;

static Point mouse_pos = {0};

void _window_resize(user_window_t* window, Size new_size, bool inform_owner);
void _write_window_title(user_window_t* window);

Screen _screen = {0};

ca_layer* _g_background = NULL;
array_t* _g_timers = NULL;

Screen* gfx_screen() {
	if (_screen.pmem != NULL) return &_screen;
	return NULL;
}

static void handle_keystroke(amc_message_t* keystroke_msg) {
	key_event_t* event = (key_event_t*)keystroke_msg->body;

	if (event->type == KEY_PRESSED) {
		// Hack: Tab switches windows
		if (event->key == '\t') {
			window_move_to_top(windows_get_bottom_window());
		}
	}

	// Only send this keystroke to the foremost program
	user_window_t* top_window = windows_get_top_window();
	if (top_window) {
		uint32_t awm_event = event->type == KEY_PRESSED ? AWM_KEY_DOWN : AWM_KEY_UP;
		window_handle_keyboard_event(top_window, awm_event, event->key);
	}
}

typedef enum window_resize_edge {
	WINDOW_RESIZE_EDGE_LEFT = 0,
	WINDOW_RESIZE_EDGE_NOT_LEFT = 1
} window_resize_edge_t;

typedef struct mouse_interaction_state {
	bool left_click_down;
	user_window_t* active_window;
	desktop_shortcut_t* hovered_shortcut;

	// Drag state
	bool has_begun_drag;
	bool is_resizing_top_window;
	bool is_moving_top_window;
	window_resize_edge_t resize_edge;
	bool is_dragging_shortcut;

	bool is_prospective_window_move;
	bool is_prospective_window_resize;
	Point mouse_pos;
} mouse_interaction_state_t;

static mouse_interaction_state_t g_mouse_state = {0};

static void _begin_left_click(mouse_interaction_state_t* state, Point mouse_point) {
	state->left_click_down = true;

	if (!state->active_window) {
		printf("Left click on background\n");
		// Did the user click on a background icon?
		if (state->hovered_shortcut) {
			printf("Left click with hover shortcut %d\n", state->hovered_shortcut->in_soft_click);
			if (state->hovered_shortcut->in_soft_click) {
				uint32_t now = ms_since_boot();
				uint32_t elapsed_ms = now - state->hovered_shortcut->first_click_start_time;
				if (elapsed_ms > 500) {
					printf("Click after double-click timed out, considering as first-click %d %d %d\n", elapsed_ms, now, state->hovered_shortcut->first_click_start_time);
					state->hovered_shortcut->first_click_start_time = now;
				}
				else {
					state->hovered_shortcut->in_soft_click = false;
					//if (!amc_service_is_active(CRASH_REPORTER_SERVICE_NAME)) {
						file_manager_launch_file_request_t req = {0};
						req.event = FILE_MANAGER_LAUNCH_FILE;
						snprintf(req.path, sizeof(req.path), state->hovered_shortcut->program_path);
						amc_message_construct_and_send(FILE_MANAGER_SERVICE_NAME, &req, sizeof(file_manager_launch_file_request_t));
					//}
				}
				return;
			}
			else {
				desktop_shortcut_highlight(state->hovered_shortcut);
			}
		}
		return;
	}
	// Left click within a window
	// Set it to the topmost window if it wasn't already
	if (windows_get_top_window() != state->active_window) {
		printf("Left click in background window %s. Move to top\n", state->active_window->owner_service);
		state->active_window = window_move_to_top(state->active_window);
	}
	else {
		printf("Left click on topmost window %s\n", state->active_window->owner_service);
	}

	// Send the click event to the window
	Point mouse_within_window = point_translate(mouse_point, state->active_window->frame);
	window_handle_left_click(state->active_window, mouse_within_window);
}

static void _end_left_click(mouse_interaction_state_t* state, Point mouse_point) {
	printf("End left click\n");
	state->left_click_down = false;
	if (state->active_window) {
		Point mouse_within_window = point_translate(mouse_point, state->active_window->frame);
		window_handle_left_click_ended(state->active_window, mouse_within_window);
	}
}

static void _exit_hover_window(mouse_interaction_state_t* state) {
	printf("Mouse exited %s\n", state->active_window->owner_service);
	window_handle_mouse_exited(state->active_window);

	state->active_window = NULL;
}

static void _enter_hover_window(mouse_interaction_state_t* state, user_window_t* window) {
	printf("Mouse entered %s\n", window->owner_service);
	window_handle_mouse_entered(window);

	// Keep track that we're currently within this window
	state->active_window = window;
}

static void _exit_hovered_shortcut(mouse_interaction_state_t* state) {
	printf("Mouse exited shortcut %s\n", state->hovered_shortcut->program_path);
	desktop_shortcut_handle_mouse_exited(state->hovered_shortcut);
	state->hovered_shortcut = NULL;
}

static void _enter_hovered_shortcut(mouse_interaction_state_t* state, desktop_shortcut_t* shortcut) {
	printf("Mouse entered shortcut %s\n", shortcut->program_path);
	desktop_shortcut_handle_mouse_entered(shortcut);
	// Keep track that we're currently within this shortcut
	state->hovered_shortcut = shortcut;
}

static void _mouse_reset_prospective_action_flags(mouse_interaction_state_t* state) {
	// Reset parameters about what actions the mouse may take based on its current position
	state->is_prospective_window_move = false;
	state->is_prospective_window_resize = false;
	if (state->active_window != NULL) {
		Point mouse_within_window = point_translate(state->mouse_pos, state->active_window->frame);
		Rect content_view = state->active_window->content_view->frame;
		int inset = 8;
		Rect content_view_inset = rect_make(
			point_make(
				content_view.origin.x + inset,
				content_view.origin.y + inset
			),
			size_make(
				content_view.size.width - (inset * 2),
				content_view.size.height - (inset * 2)
			)
		);

		Rect title_bar_frame = rect_make(
			point_zero(),
			size_make(state->active_window->frame.size.width, WINDOW_TITLE_BAR_HEIGHT)
		);
		if (rect_contains_point(title_bar_frame, mouse_within_window)) {
			state->is_prospective_window_move = true;
		}
		else if (!rect_contains_point(content_view_inset, mouse_within_window)) {
			state->is_prospective_window_resize = true;
		}

		if (rect_contains_point(title_bar_frame, mouse_within_window)) {
			// TODO(PT): Keep track of what we last drew and only redraw if this is a state change
			window_redraw_title_bar(state->active_window, true);
		}
		else {
			window_redraw_title_bar(state->active_window, false);
		}
	}
}

static void _moved_in_hover_window(mouse_interaction_state_t* state, Point mouse_point) {
	if (!state->is_prospective_window_move && !state->is_prospective_window_resize) {
		Point mouse_within_window = point_make(
			mouse_point.x - rect_min_x(state->active_window->frame),
			mouse_point.y - rect_min_y(state->active_window->frame)
		);
		window_handle_mouse_moved(state->active_window, mouse_within_window);
	}
}

static void _handle_mouse_moved(mouse_interaction_state_t* state, Point mouse_point, int32_t delta_x, int32_t delta_y, int32_t delta_z) {
	// Check if we've moved outside the bounds of the hover window
	if (state->active_window != NULL) {
		if (!rect_contains_point(state->active_window->frame, mouse_point)) {
			_exit_hover_window(state);
		}
	}
	// Check if we've moved outside the bounds of the hovered icon
	if (state->hovered_shortcut != NULL) {
		if (!rect_contains_point(state->hovered_shortcut->view->frame, mouse_point)) {
			_exit_hovered_shortcut(state);
		}
	}

	// Check each window and see if we've just entered it
	// This array is sorted in Z-order so we encounter the topmost window first
	user_window_t* window_under_mouse = window_containing_point(mouse_point, true);
	// Is this a different window from the one we were previously hovered over?
	if (state->active_window != window_under_mouse) {
		if (state->active_window != NULL) {
			_exit_hover_window(state);
		}
		if (state->hovered_shortcut) {
			_exit_hovered_shortcut(state);
		}

		if (window_under_mouse) {
			// Inform the window we've just entered it
			_enter_hover_window(state, window_under_mouse);
		}
	}

	// If we're still within a hover window, inform it that the mouse has moved
	if (state->active_window) {
		_moved_in_hover_window(state, mouse_point);
	}
	else {
		if (window_under_mouse == NULL) {
			desktop_shortcut_t* shortcut = desktop_shortcut_containing_point(mouse_point);
			if (state->hovered_shortcut != shortcut) {
				if (state->hovered_shortcut != NULL) {
					_exit_hovered_shortcut(state);
				}
				if (shortcut != NULL) {
					_enter_hovered_shortcut(state, shortcut);
				}
			}
		}
	}
}

static void _begin_mouse_drag(mouse_interaction_state_t* state, Point mouse_point) {
	state->has_begun_drag = true;
	if (state->active_window == NULL) {
		if (state->hovered_shortcut != NULL) {
			printf("Start drag on hovered icon\n");
			state->is_dragging_shortcut = true;
		}
		else {
			printf("Start drag on desktop background\n");
		}
		return;
	}

	Point local_mouse = point_make(
		mouse_point.x - state->active_window->frame.origin.x,
		mouse_point.y - state->active_window->frame.origin.y
	);
	if (state->is_prospective_window_move) {
		state->is_moving_top_window = true;
		if (rect_contains_point(state->active_window->close_button_frame, local_mouse)) {
			amc_msg_u32_1__send(state->active_window->owner_service, AWM_CLOSE_WINDOW_REQUEST);
		}
	}
	else if (state->is_prospective_window_resize) {
		state->is_resizing_top_window = true;
		// Which side of the window are we dragging on?
		if (local_mouse.x < 10) {
			state->resize_edge = WINDOW_RESIZE_EDGE_LEFT;
		}
		else {
			state->resize_edge = WINDOW_RESIZE_EDGE_NOT_LEFT;
		}
	}
	else {
		printf("Start drag within content view\n");
	}
}

static void _end_mouse_drag(mouse_interaction_state_t* state, Point mouse_point) {
	if (state->has_begun_drag) {
		printf("End drag\n");
		if (state->is_dragging_shortcut) {
			Rect original_frame = state->hovered_shortcut->view->frame;
			desktop_shortcut_grid_slot_t* slot = desktop_shortcut_grid_slot_for_rect(original_frame);
			// If we couldn't find a good place to put the icon, move it back to its original spot
			if (!slot) {
				printf("Failed to find a good place to put shortcut, returning to original slot\n");
				slot = state->hovered_shortcut->grid_slot;
			}
			awm_animation_snap_shortcut_t* anim = awm_animation_snap_shortcut_init(32, state->hovered_shortcut, slot);
			awm_animation_start((awm_animation_t*)anim);
		}

		state->has_begun_drag = false;
		state->is_moving_top_window = false;
		state->is_resizing_top_window = false;
		state->is_dragging_shortcut = false;

		//compositor_queue_rect_difference_to_redraw(original_frame, window->frame);
		//windows_invalidate_drawable_regions_in_rect(rect_union(original_frame, window->frame));
	}
}

static Rect rect_bind_to_screen_frame(Rect inp) {
	inp.origin.x = max(inp.origin.x, 0);
	inp.origin.y = max(inp.origin.y, 0);

	if (inp.origin.x + inp.size.width >= _screen.resolution.width) {
		uint32_t overhang = inp.origin.x + inp.size.width - _screen.resolution.width;
		inp.origin.x -= overhang;
	}
	if (inp.origin.y + inp.size.height >= _screen.resolution.height) {
		uint32_t overhang = inp.origin.y + inp.size.height - _screen.resolution.height;
		inp.origin.y -= overhang;
	}
	return inp;
}

static void _adjust_window_position(user_window_t* window, int32_t delta_x, int32_t delta_y) {
	Rect original_frame = window->frame;

	window->frame.origin.x += delta_x;
	window->frame.origin.y += delta_y;

	// Don't let the window go off-screen
	window->frame = rect_bind_to_screen_frame(window->frame);

	if (window->frame.origin.x + window->frame.size.width >= _screen.resolution.width) {
		uint32_t overhang = window->frame.origin.x + window->frame.size.width - _screen.resolution.width;
		window->frame.origin.x -= overhang;
	}
	if (window->frame.origin.y + window->frame.size.height >= _screen.resolution.height) {
		uint32_t overhang = window->frame.origin.y + window->frame.size.height - _screen.resolution.height;
		window->frame.origin.y -= overhang;
	}

	compositor_queue_rect_difference_to_redraw(original_frame, window->frame);
	windows_invalidate_drawable_regions_in_rect(rect_union(original_frame, window->frame));
}

static void _handle_mouse_dragged(mouse_interaction_state_t* state, Point mouse_point, int32_t delta_x, int32_t delta_y, int32_t delta_z) {
	// Nothing to do if we didn't start the drag with a hover window
	if (!state->active_window) {
		if (state->hovered_shortcut) {
			Rect original_frame = state->hovered_shortcut->view->frame;
			Rect new_frame = rect_make(
				point_make(
					rect_min_x(original_frame) + delta_x,
					rect_min_y(original_frame) + delta_y
				),
				original_frame.size
			);
			// Don't let the shortcut go off-screen
			new_frame = rect_bind_to_screen_frame(new_frame);
			state->hovered_shortcut->view->frame = new_frame;

			Rect total_update_frame = rect_union(original_frame, new_frame);
			compositor_queue_rect_difference_to_redraw(original_frame, new_frame);
			windows_invalidate_drawable_regions_in_rect(total_update_frame);
		}
		else {
			printf("Drag on desktop background\n");
		}
		return;
	}
	
	if (state->is_moving_top_window) {
		_adjust_window_position(state->active_window, delta_x, delta_y);
	}
	else if (state->is_resizing_top_window) {
		Size new_size = state->active_window->frame.size;

		// Dragging left from the left edge?
		if (state->resize_edge == WINDOW_RESIZE_EDGE_LEFT) {
			if (delta_x > 0) {
				new_size.width += delta_x;
			}
			else {
				// If the window is dragged to the left from the left edge,
				// move its origin and make it bigger
				new_size.width += -delta_x;
				_adjust_window_position(state->active_window, delta_x, 0);
			}

			if (delta_y > 0) {
				new_size.height += delta_y;
			}
			else {
				// If the window is dragged upwards from the left edge,
				// move its origin and make it bigger
				new_size.height += -delta_y;
				_adjust_window_position(state->active_window, 0, delta_y);
			}
		}
		else {
			new_size.width += delta_x;
			new_size.height += delta_y;
		}

		// Don't let the window get too small
		new_size.width = max(new_size.width, (WINDOW_BORDER_MARGIN * 2) + 20);
		new_size.height = max(new_size.height, WINDOW_TITLE_BAR_HEIGHT + (WINDOW_BORDER_MARGIN * 2) + 20);
		// Or too big...
		new_size.width = min(new_size.width, state->active_window->layer->size.width);
		new_size.height = min(new_size.height, state->active_window->layer->size.height);

		_window_resize(state->active_window, new_size, true);
	}
	else {
		// Drag within content view
		// Mouse is hovered within the content view
		Point local_mouse = point_make(
			mouse_point.x - state->active_window->frame.origin.x,
			mouse_point.y - state->active_window->frame.origin.y
		);
		local_mouse.x -= state->active_window->content_view->frame.origin.x;
		local_mouse.y -= state->active_window->content_view->frame.origin.y;
		amc_msg_u32_3__send(state->active_window->owner_service, AWM_MOUSE_DRAGGED, local_mouse.x, local_mouse.y);
	}
}

static void _handle_mouse_scroll(mouse_interaction_state_t* state, int32_t delta_z) {
	if (state->active_window) {
		// Scroll within a window
		// Inform the window
		amc_msg_u32_2__send(state->active_window->owner_service, AWM_MOUSE_SCROLLED, (uint32_t)delta_z);
	}
}

static void mouse_dispatch_events(uint8_t status_byte, Point mouse_point, int32_t delta_x, int32_t delta_y, int32_t delta_z) {
	mouse_interaction_state_t* mouse_state = &g_mouse_state;

	// Previous mouse position should be redrawn
	compositor_queue_rect_to_redraw(rect_make(mouse_state->mouse_pos, size_make(14, 14)));

	mouse_state->mouse_pos = mouse_point;

	// Is the left button clicked?
	if (status_byte & (1 << 0)) {
		// Were we already tracking a left-click?
		if (!mouse_state->left_click_down) {
			_begin_left_click(mouse_state, mouse_point);
			_begin_mouse_drag(mouse_state, mouse_point);
		}
		else {
			// The left mouse button is still held down - we're in a drag
			_handle_mouse_dragged(mouse_state, mouse_point, delta_x, delta_y, delta_z);
			return;
		}
	}
	else {
		// Did we just release a left click?
		if (mouse_state->left_click_down) {
			_end_left_click(mouse_state, mouse_point);
			_end_mouse_drag(mouse_state, mouse_point);
		}
	}

	// Did we scroll?
	if (delta_z != 0) {
		_handle_mouse_scroll(mouse_state, delta_z);
	}

	// Now that all other event types have been processed, check to see if we've just
	// moved to a different window
	_handle_mouse_moved(mouse_state, mouse_point, delta_x, delta_y, delta_z);
}

static bool handle_mouse_event(amc_message_t* mouse_event, incremental_mouse_state_t* incremental_update) {
	int8_t state = mouse_event->body[0];
	int8_t rel_x = mouse_event->body[1];
	int8_t rel_y = mouse_event->body[2];
	int8_t rel_z = mouse_event->body[3];

	mouse_pos.x += rel_x;
	mouse_pos.y += rel_y;
	// Bind mouse to screen dimensions
	mouse_pos.x = max(0, mouse_pos.x);
	mouse_pos.y = max(0, mouse_pos.y);
	mouse_pos.x = min(_screen.resolution.width - 20, mouse_pos.x);
	mouse_pos.y = min(_screen.resolution.height - 20, mouse_pos.y);

	bool updated_state_byte = state == incremental_update->state;
	incremental_update->state = state;
	incremental_update->rel_x += rel_x;
	incremental_update->rel_y += rel_y;
	incremental_update->rel_z += rel_z;
	incremental_update->combined_msg_count += 1;
	return !updated_state_byte;
}

Rect _draw_cursor(ca_layer* dest) {
	mouse_interaction_state_t* mouse_state = &g_mouse_state;
	Color mouse_color = color_green();
	bool is_resize = false;
	if (mouse_state->is_resizing_top_window) {
		mouse_color = color_make(207, 25, 185);
		is_resize = true;
	}
	else if (mouse_state->is_moving_top_window) {
		mouse_color = color_make(30, 65, 217);
	}
	else if (mouse_state->is_prospective_window_resize) {
		mouse_color = color_make(212, 119, 201);
		is_resize = true;
	}
	else if (mouse_state->is_prospective_window_move) {
		mouse_color = color_make(121, 160, 217);
	}

	// Draw the new cursor
	Size cursor_size = size_make(14, 14);
	Rect new_mouse_rect = rect_make(mouse_pos, cursor_size);
	draw_rect(dest, new_mouse_rect, color_black(), THICKNESS_FILLED);
	draw_rect(
		dest, 
		rect_make(
			point_make(new_mouse_rect.origin.x + 2, new_mouse_rect.origin.y + 2), 
			size_make(10, 10)
		), 
		mouse_color, 
		THICKNESS_FILLED
	);

	return new_mouse_rect;
}

void _window_resize(user_window_t* window, Size new_size, bool inform_window) {
	Rect original_frame = window->frame;
	window->frame.size = new_size;

	// Resize the text box
	Size title_bar_size = size_make(new_size.width, WINDOW_TITLE_BAR_HEIGHT);

	// The content view is a bit smaller to accomodate decorations
	window->content_view->frame = rect_make(
		point_make(
			WINDOW_BORDER_MARGIN, 
			// The top edge does not have a margin
			title_bar_size.height
		), 
		size_make(
			new_size.width - (WINDOW_BORDER_MARGIN * 2),
			// No need to multiply margin by 2 since the top edge doesn't have a margin
			new_size.height - WINDOW_BORDER_MARGIN - title_bar_size.height
		)
	);

	window_redraw_title_bar(window, false);

	compositor_queue_rect_difference_to_redraw(original_frame, window->frame);
	windows_invalidate_drawable_regions_in_rect(rect_union(original_frame, window->frame));

	if (inform_window && !window->remote_process_died) {
		awm_window_resized_msg_t msg = {0};
		msg.event = AWM_WINDOW_RESIZED;
		msg.new_size = window->content_view->frame.size;
		amc_message_construct_and_send(window->owner_service, &msg, sizeof(msg));
	}
}

static void _update_window_title(const char* owner_service, awm_window_title_msg_t* title_msg) {
	printf("update_window_title %s %s\n", owner_service, title_msg->title);
	user_window_t* window = window_with_service_name(owner_service);
	if (!window) {
		printf("Failed to find a window for %s\n", owner_service);
		return;
	}

	if (window->title) {
		free((char*)window->title);
	}
	window->title = strndup(title_msg->title, title_msg->len);
	_window_resize(window, window->frame.size, false);
}

static void _remove_and_teardown_window_for_service(const char* owner_service) {
	user_window_t* window = window_with_service_name(owner_service);
	if (window == NULL) {
		return;
	}
	// Make sure we don't try to fetch the remote layer anymore
	window->remote_process_died = true;

	if (g_mouse_state.active_window == window) {
		printf("Clear active window 0x%08x\n", window);
		g_mouse_state.active_window = NULL;
	}

	awm_animation_close_window_t* anim = awm_animation_close_window_init(200, window);
	awm_animation_start((awm_animation_t*)anim);
}

static void handle_user_message(amc_message_t* user_message) {
	const char* source_service = amc_message_source(user_message);
	uint32_t command = amc_msg_u32_get_word(user_message, 0);

	if (!strncmp(source_service, PREFERENCES_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
		if (command == AWM_PREFERENCES_UPDATED) {
			prefs_updated_msg_t* msg = (prefs_updated_msg_t*)&user_message->body;
			printf("AWM updating background gradient... %d %d %d, %d %d %d\n",msg->from.val[0], msg->from.val[1], msg->from.val[2], msg->to.val[0], msg->to.val[1], msg->to.val[2]);
			radial_gradiant(
				_g_background, 
				_g_background->size, 
				msg->from,
				msg->to,
				_g_background->size.width/2.0, 
				_g_background->size.height/2.0, 
				(float)_g_background->size.height * 0.65
			);
			compositor_queue_rect_to_redraw(rect_make(point_zero(), _g_background->size));
			// Also re-render each desktop shortcut, as they use the background color in their rendering
			array_t* shortcuts = desktop_shortcuts();
			for (int32_t i = 0; i < shortcuts->size; i++) {
				desktop_shortcut_t* shortcut = array_lookup(shortcuts, i);
				desktop_shortcut_render(shortcut);
			}
			return;
		}
	}
	else if (!strncmp(source_service, FILE_MANAGER_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
		if (command == FILE_MANAGER_READY) {
			windows_fetch_resource_images();
			return;
		}
	}
	// User requesting a window to draw in to?
	if (command == AWM_REQUEST_WINDOW_FRAMEBUFFER) {
		uint32_t width = amc_msg_u32_get_word(user_message, 1);
		uint32_t height = amc_msg_u32_get_word(user_message, 2);
		window_create(source_service, width, height);
		return;
	}
	else if (command == AWM_WINDOW_REDRAW_READY) {
		user_window_t* window = window_with_service_name(source_service);
		window_queue_fetch(window);
	}
	else if (command == AWM_WINDOW_REDRAW_PARTIAL) {
		user_window_t* window = window_with_service_name(source_service);
		awm_window_update_partial_t* partial_update = (awm_window_update_partial_t*)user_message->body;
		window_queue_partial_fetch(window, partial_update);
	}
	else if (command == AWM_UPDATE_WINDOW_TITLE) {
		awm_window_title_msg_t* title_msg =  (awm_window_title_msg_t*)user_message->body;
		_update_window_title(source_service, title_msg);
	}
	else if (command == AWM_CLOSE_WINDOW) {
		//_remove_and_teardown_window_for_service(source_service);
		printf("Received AWM_CLOSE_WINDOW from %s\n", source_service);
	}
	else {
		printf("Unknown message from %s: %d\n", source_service, command);
	}
}

struct mallinfo_s {
	int arena;    /* total space allocated from system */
	int ordblks;  /* number of non-inuse chunks */
	int smblks;   /* unused -- always zero */
	int hblks;    /* number of mmapped regions */
	int hblkhd;   /* total space in mmapped regions */
	int usmblks;  /* unused -- always zero */
	int fsmblks;  /* unused -- always zero */
	int uordblks; /* total allocated space */
	int fordblks; /* total non-inuse space */
	int keepcost; /* top-most, releasable (via malloc_trim) space */
};	
struct mallinfo_s mallinfo();

void print_memory(void) {
	struct mallinfo_s p = mallinfo();
	printf("Heap space: 		 0x%08x\n", p.arena);
	printf("Total allocd space : 0x%08x\n", p.uordblks);
	printf("Total free space   : 0x%08x\n", p.fordblks);
}

ca_layer* desktop_background_layer(void) {
	return _g_background;
}

ca_layer* video_memory_layer(void) {
	return _screen.vmem;
}

ca_layer* physical_video_memory_layer(void) {
	return _screen.pmem;
}

static void _awm_init(void) {
	amc_register_service(AWM_SERVICE_NAME);
	
	// Init state for the desktop
	windows_init();
	animations_init();
	compositor_init();
	_g_timers = array_create(32);

	// Ask the kernel to map in the framebuffer and send us info about it
	amc_msg_u32_1__send(AXLE_CORE_SERVICE_NAME, AMC_AWM_MAP_FRAMEBUFFER);

	amc_message_t* msg;
	amc_message_await(AXLE_CORE_SERVICE_NAME, &msg);
	uint32_t event = amc_msg_u32_get_word(msg, 0);
	assert(event == AMC_AWM_MAP_FRAMEBUFFER_RESPONSE, "Expected awm framebuffer info");
	amc_framebuffer_info_t* framebuffer_info = (amc_framebuffer_info_t*)msg->body;
	printf("Recv'd framebuf info!\n");
    printf("0x%08x 0x%08x (%d x %d x %d x %d)\n", framebuffer_info->address, framebuffer_info->size, framebuffer_info->width, framebuffer_info->height, framebuffer_info->bytes_per_pixel, framebuffer_info->bits_per_pixel);

	// Set up the screen object
    _screen.resolution = size_make(framebuffer_info->width, framebuffer_info->height);
    _screen.bits_per_pixel = framebuffer_info->bits_per_pixel;
    _screen.bytes_per_pixel = framebuffer_info->bytes_per_pixel;

    _screen.video_memory_size = framebuffer_info->size;
	_screen.pmem = calloc(1, sizeof(ca_layer));
	_screen.pmem->size = _screen.resolution;
	_screen.pmem->raw = (uint8_t*)framebuffer_info->address;
	_screen.pmem->alpha = 1.0;

	_screen.vmem = create_layer(screen_resolution());
	printf("awm framebuffer: %d x %d, %d BPP @ 0x%08x\n", _screen.resolution.width, _screen.resolution.height, _screen.bits_per_pixel, _screen.pmem->raw);

	// Set up the desktop background
	Rect screen_frame = rect_make(point_zero(), _screen.resolution);
	_g_background = create_layer(screen_frame.size);
	radial_gradiant(
		_g_background, 
		screen_frame.size, 
		color_make(2, 184, 255),
		color_make(39, 67, 255), 
		rect_mid_x(screen_frame),
		rect_mid_y(screen_frame),
		(float)_g_background->size.height * 0.65
	);
}

static void _awm_process_amc_messages(bool should_block) {
	amc_message_t* msg;
	incremental_mouse_state_t incremental_mouse_update = {0};
	memset(&incremental_mouse_update, 0, sizeof(incremental_mouse_state_t));
	incremental_mouse_update.combined_msg_count = 0;

	if (!should_block) {
		if (!amc_has_message()) {
			return;
		}
	}

	do {
		// Wait until we've unblocked with at least one message available
		amc_message_await_any(&msg);

		// Will automatically respond to watchdog pings
		if (libamc_handle_message(msg)) {
			continue;
		}
		const char* source_service = amc_message_source(msg);

		// Always update the prospective mouse action flags when the event loop runs
		_mouse_reset_prospective_action_flags(&g_mouse_state);

		// Process the message we just received
		if (!strcmp(source_service, KB_DRIVER_SERVICE_NAME)) {
			handle_keystroke(msg);
			continue;
		}
		else if (!strcmp(source_service, "com.axle.mouse_driver")) {
			// Update the mouse position based on the data packet
			bool changed_state = handle_mouse_event(msg, &incremental_mouse_update);
			if (changed_state) {
				// TODO(PT): Perhaaps we can drop the combined-mouse-update optimisation
				mouse_dispatch_events(
					incremental_mouse_update.state, 
					mouse_pos, 
					incremental_mouse_update.rel_x,
					incremental_mouse_update.rel_y,
					incremental_mouse_update.rel_z
				);
				memset(&incremental_mouse_update, 0, sizeof(incremental_mouse_state_t));
				incremental_mouse_update.combined_msg_count = 0;
			}
		}
		else if (!strncmp(source_service, AXLE_CORE_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
			uint32_t* u32buf = (uint32_t*)&msg->body;
			if (u32buf[0] == AMC_SERVICE_DIED_NOTIFICATION) {
				amc_service_died_notification_t* notif = (amc_service_died_notification_t*)&msg->body;
				_remove_and_teardown_window_for_service(notif->dead_service);
				// Ask amc to delete any messages awm sent to this program
				amc_flush_messages_to_service_cmd_t req = {0};
				req.event = AMC_FLUSH_MESSAGES_TO_SERVICE;
				snprintf((char*)&req.remote_service, sizeof(req.remote_service), notif->dead_service);
				amc_message_construct_and_send(AXLE_CORE_SERVICE_NAME, &req, sizeof(req));
			}
			else {
				printf("Unknown message from core: %d\n", u32buf[0]);
				assert(false, "Unknown message from core");
			}
		}
		else {
			// TODO(PT): If a window sends REDRAW_READY, we can put it onto a "ready to redraw" list
			// Items can be popped off the list based on their Z-index, or a periodic time-based update
			// TODO(PT): If a window has requested multiple redraws within a single awm event loop
			// pass, awm should redraw it only once
			handle_user_message(msg);
		}
	} while (amc_has_message());
}

typedef enum timers_state {
	TIMERS_LATE = 0,
	SLEPT_FOR_TIMERS = 1,
	NO_TIMERS = 2
} timers_state_t;

static timers_state_t _sleep_for_timers(void) {
	if (_g_timers->size == 0) {
		return NO_TIMERS;
	}
	uint32_t next_fire_date = 0;
	for (uint32_t i = 0; i < _g_timers->size; i++) {
        awm_timer_t* t = array_lookup(_g_timers, i);
		if (!next_fire_date || t->fires_after < next_fire_date) {
			next_fire_date = t->fires_after;
		}
    }
	int32_t time_to_next_fire = next_fire_date - ms_since_boot();
	if (time_to_next_fire <= 0) {
		return TIMERS_LATE;
	}

	uint32_t b[2];
    b[0] = AMC_SLEEP_UNTIL_TIMESTAMP_OR_MESSAGE;
    b[1] = time_to_next_fire;
    amc_message_construct_and_send(AXLE_CORE_SERVICE_NAME, &b, sizeof(b));

	return SLEPT_FOR_TIMERS;
}

void awm_timer_start(uint32_t duration, awm_timer_cb_t timer_cb, void* invoke_ctx) {
    awm_timer_t* t = calloc(1, sizeof(awm_timer_t));
    t->start_time = ms_since_boot();
    t->duration = duration;
    t->fires_after = t->start_time + duration;
    t->invoke_cb = timer_cb;
    t->invoke_ctx = invoke_ctx;
    array_insert(_g_timers, t);
}

static void _dispatch_ready_timers(void) {
    uint32_t now = ms_since_boot();
    for (int32_t i = 0; i < _g_timers->size; i++) {
        awm_timer_t* t = array_lookup(_g_timers, i);
        if (t->fires_after <= now) {
            uint32_t late_by = now - (t->start_time + t->duration);
            t->invoke_cb(t->invoke_ctx);
        }
    }
    if (_g_timers->size > 0) {
        for (int32_t i = _g_timers->size - 1; i >= 0; i--) {
            awm_timer_t* t = array_lookup(_g_timers, i);
            if (t->fires_after <= now) {
                array_remove(_g_timers, i);
                free(t);
            }
        }
    }
}

static void _awm_enter_event_loop(void) {
	// Draw the background onto the screen buffer to start off
	Rect screen_frame = rect_make(point_zero(), screen_resolution());
	blit_layer(_screen.vmem, _g_background, screen_frame, screen_frame);
	blit_layer(_screen.pmem, _screen.vmem, screen_frame, screen_frame);

	while (true) {
		timers_state_t timers_state = _sleep_for_timers();
		_awm_process_amc_messages(timers_state == NO_TIMERS);
		_dispatch_ready_timers();
		compositor_render_frame();
	}
}

int main(int argc, char** argv) {
	_awm_init();
	_awm_enter_event_loop();
	return 0;
}

void print_free_areas(array_t* unobstructed_area) {
	for (int32_t i = 0; i < unobstructed_area->size; i++) {
		Rect* r = array_lookup(unobstructed_area, i);
		printf("\t\t\trect(%d, %d, %d, %d);\n", r->origin.x, r->origin.y, r->size.width, r->size.height);
	}
}
