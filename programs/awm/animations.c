#include <stddef.h>

#include "math.h"
#include "composite.h"
#include "animations.h"

array_t* _g_pending_animations = NULL;

float lerp(float a, float b, float f) {
    return a + f * (b - a);
}


// Close window animation

static void _awm_animation_close_window_step(awm_animation_close_window_t* anim, float percent) {
	user_window_t* window = anim->window;
	window->layer->alpha = 1.0 - percent;

    Rect current_frame = window->frame;

	Rect from = anim->original_frame;
	Rect to = anim->destination_frame;
	Rect new_frame = rect_make(
		point_make(
			lerp(rect_min_x(from), rect_min_x(to), percent),
			lerp(rect_min_y(from), rect_min_y(to), percent)
		),
		size_make(
			lerp(from.size.width, to.size.width, percent),
			lerp(from.size.height, to.size.height, percent)
		)
	);
	window->frame = new_frame;
	_window_resize(window, window->frame.size, false);
	//window_render_scaled_content_layer(window);

	Rect total_update_frame = rect_union(current_frame, new_frame);
	compositor_queue_rect_difference_to_redraw(current_frame, new_frame);
	windows_invalidate_drawable_regions_in_rect(total_update_frame);

	// TODO(PT): Also need to update the windows in front of this one? How?
}

static void _awm_animation_close_window_finish(awm_animation_close_window_t* anim) {
	printf("_window_close_animation completed\n");
	user_window_t* window = anim->window;
	window_destroy(window);
}

awm_animation_close_window_t* awm_animation_close_window_init(uint32_t duration, user_window_t* window) {
	awm_animation_close_window_t* anim = calloc(1, sizeof(awm_animation_close_window_t));
	uint32_t now = ms_since_boot();
	anim->base.start_time = now;
	anim->base.end_time = now + duration;
	anim->base.step_cb = (awm_animation_step_cb)_awm_animation_close_window_step;
	anim->base.finish_cb = (awm_animation_finish_cb)_awm_animation_close_window_finish;
	anim->window = window;

	Size screen_size = screen_resolution();
	Size final_size = size_make(
		screen_size.height / 10.0,
		screen_size.width / 10.0
	);
	anim->original_frame = window->frame;
	anim->destination_frame = rect_make(
		point_make(
			(screen_size.width / 2.0) - (final_size.width / 2.0),
			screen_size.height - final_size.height
		),
		final_size
	);
	//window->should_scale_layer = true;

	return anim;
}

// Open window animation

static void _awm_animation_open_window_step(awm_animation_open_window_t* anim, float percent) {
	user_window_t* window = anim->window;

	window->layer->alpha = percent;

    Rect current_frame = window->frame;

	Rect from = anim->original_frame;
	Rect to = anim->destination_frame;
	Rect new_frame = rect_make(
		point_make(
			lerp(rect_min_x(from), rect_min_x(to), percent),
			lerp(rect_min_y(from), rect_min_y(to), percent)
		),
		size_make(
			lerp(from.size.width, to.size.width, percent),
			lerp(from.size.height, to.size.height, percent)
		)
	);
	window->frame = new_frame;
	_window_resize(window, window->frame.size, true);

	Rect total_update_frame = rect_union(current_frame, new_frame);
	compositor_queue_rect_difference_to_redraw(current_frame, new_frame);
	windows_invalidate_drawable_regions_in_rect(total_update_frame);
}

static void _awm_animation_open_window_finish(awm_animation_open_window_t* anim) {
	// Inform the window of its initial size
	//_window_resize(anim->window, anim->window->frame.size, true);
}

awm_animation_open_window_t* awm_animation_open_window_init(uint32_t duration, user_window_t* window, Rect dest_frame) {
	awm_animation_open_window_t* anim = calloc(1, sizeof(awm_animation_open_window_t));
	uint32_t now = ms_since_boot();
	anim->base.start_time = now;
	anim->base.end_time = now + duration;
	anim->base.step_cb = (awm_animation_step_cb)_awm_animation_open_window_step;
	anim->base.finish_cb = (awm_animation_finish_cb)_awm_animation_open_window_finish;
	anim->window = window;
	anim->destination_frame = dest_frame;

	Size screen_size = screen_resolution();
	Size original_size = size_make(
		screen_size.height / 10.0,
		screen_size.width / 10.0
	);
	anim->original_frame = rect_make(
		point_make(
			(screen_size.width / 2.0) - (original_size.width / 2.0),
			screen_size.height - original_size.height
		),
		original_size
	);

	//_window_resize(anim->window, anim->destination_frame.size, true);
	window->frame = anim->original_frame;

	return anim;
}

// Snap shortcut animation

static void _awm_animation_snap_shortcut_step(awm_animation_snap_shortcut_t* anim, float percent) {
    Rect from = anim->original_frame;
    Rect to = anim->destination_frame;

    Rect current_frame = anim->shortcut->view->frame;
	Rect new_frame = rect_make(
		point_make(
			lerp(rect_min_x(from), rect_min_x(to), percent),
			lerp(rect_min_y(from), rect_min_y(to), percent)
		),
		size_make(
			lerp(from.size.width, to.size.width, percent),
			lerp(from.size.height, to.size.height, percent)
		)
	);
    anim->shortcut->view->frame = new_frame;
    
	Rect total_update_frame = rect_union(current_frame, new_frame);
	compositor_queue_rect_difference_to_redraw(current_frame, new_frame);
	windows_invalidate_drawable_regions_in_rect(total_update_frame);
}

static void _awm_animation_snap_shortcut_finish(awm_animation_snap_shortcut_t* anim) {
    Rect new_frame = desktop_shortcut_place_in_grid_slot(anim->shortcut, anim->dest_slot);
}

awm_animation_snap_shortcut_t* awm_animation_snap_shortcut_init(uint32_t duration, desktop_shortcut_t* shortcut, desktop_shortcut_grid_slot_t* slot) {
	awm_animation_snap_shortcut_t* anim = calloc(1, sizeof(awm_animation_snap_shortcut_t));
	uint32_t now = ms_since_boot();
	anim->base.start_time = now;
	anim->base.end_time = now + duration;
	anim->base.step_cb = (awm_animation_step_cb)_awm_animation_snap_shortcut_step;
	anim->base.finish_cb = (awm_animation_finish_cb)_awm_animation_snap_shortcut_finish;
    anim->shortcut = shortcut;
    anim->dest_slot = slot;
    anim->original_frame = shortcut->view->frame;
    anim->destination_frame = desktop_shortcut_frame_within_grid_slot(slot);
	return anim;
}

// All animations handling

static void _awm_animations_continue(uint32_t _unused) {
	uint32_t now = ms_since_boot();
	for (int32_t i = _g_pending_animations->size - 1; i >= 0; i--) {
		awm_animation_t* anim = array_lookup(_g_pending_animations, i);
		uint32_t elapsed_ms = now - anim->base.start_time;
		uint32_t duration_ms = anim->base.end_time - anim->base.start_time;
		float percent = elapsed_ms / (float)duration_ms;
		percent = min(1.0, percent);

		anim->base.step_cb(anim, percent);

		if (percent >= 1.0) {
			anim->base.finish_cb(anim);
			array_remove(_g_pending_animations, i);
			free(anim);
		}
	}
	bool animations_ongoing = _g_pending_animations->size > 0;
	if (animations_ongoing) {
		awm_timer_start(16, (awm_timer_cb_t)_awm_animations_continue, NULL);
	}
}

void awm_animation_start(awm_animation_t* anim) {
	array_insert(_g_pending_animations, anim);
	// Did we go from zero to one animations?
	if (_g_pending_animations->size == 1) {
		//awm_timer_start(0, (awm_timer_cb_t)_awm_animations_continue, NULL);
		_awm_animations_continue(NULL);
	}
}

void animations_init(void) {
	_g_pending_animations = array_create(32);
}
