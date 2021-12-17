#ifndef GUI_SLIDER_H
#define GUI_SLIDER_H

#include <libagx/lib/ca_layer.h>

#include "gui_elem.h"

typedef struct gui_slider gui_slider_t;
typedef struct gui_view gui_view_t;

typedef void(*gui_slider_percent_updated_cb_t)(gui_slider_t* s, float new_percent);

typedef struct gui_slider {
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
    gui_view_t* superview;
    float slider_percent;
    gui_slider_percent_updated_cb_t slider_percent_updated_cb;
    
    // Private fields
    uint32_t border_margin;
    bool in_left_click;
    uint32_t slidable_width;
    uint32_t slider_origin_x;
} gui_slider_t;

gui_slider_t* gui_slider_create(gui_view_t* superview, gui_window_resized_cb_t sizer_cb);
void gui_slider_destroy(gui_slider_t* view);

#endif