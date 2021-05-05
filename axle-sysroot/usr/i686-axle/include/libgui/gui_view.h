#ifndef GUI_VIEW_H
#define GUI_VIEW_H

#include <agx/lib/ca_layer.h>

#include "gui_elem.h"

typedef struct gui_view {
    // Private union members (must be first in the structure)
    gui_mouse_entered_cb_t _priv_mouse_entered_cb;
    gui_mouse_exited_cb_t _priv_mouse_exited_cb;
    gui_mouse_moved_cb_t _priv_mouse_moved_cb;
    gui_mouse_dragged_cb_t _priv_mouse_dragged_cb;
    gui_mouse_left_click_cb_t _priv_mouse_left_click_cb;
    gui_mouse_left_click_ended_cb_t _priv_mouse_left_click_ended_cb;
    gui_mouse_scrolled_cb_t _priv_mouse_scrolled_cb;
    gui_key_down_cb_t _priv_key_down_cb;
    gui_key_up_cb_t _priv_key_up_cb;
    gui_draw_cb_t _priv_draw_cb;
    _priv_gui_window_resized_cb_t _priv_window_resized_cb;
    bool _priv_needs_display;

    // Public and shared in union
    uint32_t type;
	Rect frame;
    gui_window_resized_cb_t sizer_cb;
    gui_window_t* window;

    // Public fields
    ca_layer* content_layer;
    Rect content_layer_frame;
    bool controls_content_layer;
    Color background_color;
    
    // Callbacks
    gui_mouse_entered_cb_t mouse_entered_cb;
    gui_mouse_exited_cb_t mouse_exited_cb;
    gui_mouse_moved_cb_t mouse_moved_cb;
    gui_key_down_cb_t key_down_cb;
    gui_key_up_cb_t key_up_cb;
    gui_window_resized_cb_t window_resized_cb;

    // Private fields
    array_t* subviews;
    uint32_t border_margin;
    uint32_t title_bar_height;
    char* _title;
    Rect _title_inset;
} gui_view_t;

gui_view_t* gui_view_create(gui_window_t* window, gui_window_resized_cb_t sizer_cb);
void gui_view_set_title(gui_view_t* view, char* title);
void gui_view_destroy(gui_view_t* view);

#endif