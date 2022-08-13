#ifndef AWM_ANIMATIONS_H
#define AWM_ANIMATIONS_H

#include <stdint.h>
#include "awm_internal.h"
#include "window.h"

typedef union awm_animation awm_animation_t;
typedef void (*awm_animation_step_cb)(awm_animation_t*, float percent);
typedef void (*awm_animation_finish_cb)(awm_animation_t*);

typedef struct awm_animation_base {
	uint32_t start_time;
	uint32_t end_time;
	awm_animation_step_cb step_cb;
	awm_animation_finish_cb finish_cb;
} awm_animation_base_t;

typedef struct awm_animation_close_window {
	awm_animation_base_t base;
	Rect original_frame;
    Rect destination_frame;
    user_window_t* window;
} awm_animation_close_window_t;

typedef struct awm_animation_open_window {
    awm_animation_base_t base;
	Rect original_frame;
    Rect destination_frame;
    user_window_t* window;
} awm_animation_open_window_t;

typedef struct awm_animation_snap_shortcut {
	awm_animation_base_t base;
    Rect original_frame;
    Rect destination_frame;
	desktop_shortcut_t* shortcut;
	desktop_shortcut_grid_slot_t* dest_slot;
} awm_animation_snap_shortcut_t;

typedef struct awm_animation_minimize_window {
    awm_animation_base_t base;
	Rect original_frame;
    Rect destination_frame;
    user_window_t* window;
} awm_animation_minimize_window_t;

typedef struct awm_animation_unminimize_window {
    awm_animation_base_t base;
	Rect original_frame;
    Rect destination_frame;
    user_window_t* window;
} awm_animation_unminimize_window_t;

typedef union awm_animation {
	awm_animation_base_t base;
	awm_animation_close_window_t close_window;
    awm_animation_open_window_t open_window;
	awm_animation_snap_shortcut_t snap_shortcut;
	awm_animation_minimize_window_t minimize_window;
	awm_animation_unminimize_window_t unminimize_window;
} awm_animation_t;

awm_animation_close_window_t* awm_animation_close_window_init(uint32_t duration, user_window_t* window);
awm_animation_open_window_t* awm_animation_open_window_init(uint32_t duration, user_window_t* window, Rect dest_frame);
awm_animation_open_window_t* awm_animation_open_window_init_ex(uint32_t duration, user_window_t* window, Rect dest_frame, Rect original_frame);
awm_animation_snap_shortcut_t* awm_animation_snap_shortcut_init(uint32_t duration, desktop_shortcut_t* shortcut, desktop_shortcut_grid_slot_t* slot);
awm_animation_minimize_window_t* awm_animation_minimize_window_init(uint32_t duration, user_window_t* window, Rect dest_frame, Rect original_frame);
awm_animation_unminimize_window_t* awm_animation_unminimize_window_init(uint32_t duration, user_window_t* window, Rect dest_frame, Rect original_frame);

void awm_animation_start(awm_animation_t* anim);
void animations_init(void);

#endif