#ifndef GUI_TEXT_VIEW_H
#define GUI_TEXT_VIEW_H

#include <agx/lib/text_box.h>

#include "gui_elem.h"

typedef struct gui_scrollbar gui_scrollbar_t;

typedef struct text_view {
    // Private union members (must be first in the structure)
    gui_mouse_entered_cb_t _priv_mouse_entered_cb;
    gui_mouse_exited_cb_t _priv_mouse_exited_cb;
    gui_mouse_moved_cb_t _priv_mouse_moved_cb;
    gui_mouse_dragged_cb_t _priv_mouse_dragged_cb;
    gui_mouse_left_click_cb_t _priv_mouse_left_click_cb;
    gui_mouse_left_click_ended_cb_t _priv_mouse_left_click_ended_cb;
    gui_mouse_scrolled_cb_t _priv_mouse_scrolled_cb;
    gui_key_down_cb_t _priv_key_down_cb;
    gui_draw_cb_t _priv_draw_cb;
    _priv_gui_window_resized_cb_t _priv_window_resized_cb;
    bool _priv_needs_display;

    // Public and shared in union
    uint32_t type;
	Rect frame;
    gui_window_resized_cb_t sizer_cb;
    gui_window_t* window;

    // Public fields
	text_box_t* text_box;
    
    // Callbacks
    gui_mouse_entered_cb_t mouse_entered_cb;
    gui_mouse_exited_cb_t mouse_exited_cb;
    gui_mouse_moved_cb_t mouse_moved_cb;
    uint32_t text_box_margin;

    // Private fields
    gui_scrollbar_t* scrollbar;
    uint32_t right_scrollbar_inset_origin_x;
    uint32_t right_scrollbar_inset_width;
} text_view_t;

text_view_t* gui_text_view_create(gui_window_t* window, Rect frame, Color background_color, gui_window_resized_cb_t sizer_cb);
void gui_text_view_clear(text_view_t* text_view);
void gui_text_view_clear_and_erase_history(text_view_t* text_view);
void gui_text_view_puts(text_view_t* text_view, const char* str, Color color);
void gui_text_view_destroy(text_view_t* text_view);

#endif