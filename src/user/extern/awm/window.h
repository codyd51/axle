#ifndef AWM_WINDOW_H
#define AWM_WINDOW_H

#include <agx/lib/shapes.h>
#include <agx/lib/ca_layer.h>
#include "utils.h"

typedef struct view {
	Rect frame;
	ca_layer* layer;
	array_t* drawable_rects;
	array_t* extra_draws_this_cycle;
} view_t;

typedef struct user_window {
	Rect frame;
	ca_layer* layer;
	array_t* drawable_rects;
	array_t* extra_draws_this_cycle;
	view_t* content_view;
	const char* owner_service;
	const char* title;
	Rect close_button_frame;
	bool has_done_first_draw;
} user_window_t;

#define WINDOW_BORDER_MARGIN 0
#define WINDOW_TITLE_BAR_HEIGHT 30
#define WINDOW_TITLE_BAR_VISIBLE_HEIGHT (WINDOW_TITLE_BAR_HEIGHT - 0)

user_window_t* window_move_to_top(user_window_t* window);

user_window_t* windows_get_top_window(void);
user_window_t* windows_get_bottom_window(void);
user_window_t* window_containing_point(Point point);
user_window_t* window_with_service_name(const char* service_name);

void windows_fetch_queued_windows(void);
void windows_clear_queued_windows(void);
void window_queue_fetch(user_window_t* window);
void window_queue_composite(user_window_t* window);
void window_queue_extra_draw(user_window_t* window, Rect extra);

void window_request_close(user_window_t* window);

void window_redraw_title_bar(user_window_t* window, bool close_button_hovered);

void window_handle_left_click(user_window_t* window,  Point mouse_within_window);
void window_handle_left_click_ended(user_window_t* window,  Point mouse_within_window);

void window_handle_mouse_entered(user_window_t* window);
void window_handle_mouse_exited(user_window_t* window);

void window_handle_mouse_moved(user_window_t* window, Point mouse_within_window);

void window_handle_keyboard_event(user_window_t* window, uint32_t event, uint32_t key);

void windows_init(void);
user_window_t* window_create(const char* owner_service, uint32_t width, uint32_t height);
void window_destroy(user_window_t* window);

void windows_composite(ca_layer* dest, Rect updated_rect);

void windows_invalidate_drawable_regions_in_rect(Rect r);

void windows_fetch_resource_images(void);

#endif