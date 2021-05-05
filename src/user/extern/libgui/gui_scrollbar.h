#ifndef GUI_SCROLLBAR_H
#define GUI_SCROLLBAR_H

#include "gui_elem.h"

typedef struct gui_scrollbar gui_scrollbar_t;
typedef void(*gui_scrollbar_updated_cb_t)(gui_scrollbar_t* sb, float new_scroll_percent);

typedef struct gui_scrollbar {
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
    gui_elem_t* parent;
    float scroll_percent;
    bool hidden;

    // Callbacks
    gui_scrollbar_updated_cb_t scroll_position_updated_cb;

    // Private fields
    ca_layer* layer;
    bool in_left_click;
} gui_scrollbar_t;

gui_scrollbar_t* gui_scrollbar_create(gui_window_t* window, gui_elem_t* parent, Rect frame, gui_window_resized_cb_t sizer_cb);
void gui_scrollbar_destroy(gui_scrollbar_t* scrollbar);

#endif