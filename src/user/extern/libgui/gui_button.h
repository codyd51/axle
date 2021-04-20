#ifndef GUI_BUTTON_H
#define GUI_BUTTON_H

#include <agx/lib/ca_layer.h>

#include "gui_elem.h"

typedef struct gui_button gui_button_t;
typedef struct gui_view gui_view_t;

typedef void (*gui_button_clicked_cb_t)(gui_button_t* b);

typedef struct gui_button {
    // Private union members (must be first in the structure)
    gui_mouse_entered_cb_t _priv_mouse_entered_cb;
    gui_mouse_exited_cb_t _priv_mouse_exited_cb;
    gui_mouse_moved_cb_t _priv_mouse_moved_cb;
    gui_mouse_dragged_cb_t _priv_mouse_dragged_cb;
    gui_mouse_left_click_cb_t _priv_mouse_left_click_cb;
    gui_mouse_left_click_ended_cb_t _priv_mouse_left_click_ended_cb;
    gui_mouse_scrolled_cb_t _priv_mouse_scrolled_cb;
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
    gui_button_clicked_cb_t button_clicked_cb;
    
    // Private fields
    uint32_t border_margin;
    bool in_left_click;
    char* title;
} gui_button_t;

gui_button_t* gui_button_create(gui_view_t* superview, gui_window_resized_cb_t sizer_cb, char* title);
void gui_button_destroy(gui_button_t* b);

#endif