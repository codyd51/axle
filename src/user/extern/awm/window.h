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

typedef struct desktop_shortcut {
    view_t* view;
    const char* program_path;
	const char* display_name;
	bool in_soft_click;
	bool in_mouse_hover;
} desktop_shortcut_t;

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

void desktop_view_queue_composite(view_t* view);
void desktop_view_queue_extra_draw(view_t* view, Rect extra);
void desktop_views_flush_queues(void);
array_t* desktop_views_ready_to_composite_array(void);

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

view_t* view_create(Rect frame);

void draw_queued_extra_draws(array_t* views, ca_layer* dest_layer);
void complete_queued_extra_draws(array_t* views, ca_layer* source_layer, ca_layer* dest_layer);

void draw_views_to_layer(array_t* views, ca_layer* dest_layer);

void icons_add(view_t* icon);
array_t* all_desktop_views(void);

array_t* desktop_shortcuts(void);
desktop_shortcut_t* desktop_shortcut_containing_point(Point point);
void desktop_shortcut_handle_mouse_exited(desktop_shortcut_t* shortcut);
void desktop_shortcut_handle_mouse_entered(desktop_shortcut_t* shortcut);
void desktop_shortcut_highlight(desktop_shortcut_t* shortcut);
void desktop_shortcut_unhighlight(desktop_shortcut_t* shortcut);
void desktop_shortcut_render(desktop_shortcut_t* ds);

#endif