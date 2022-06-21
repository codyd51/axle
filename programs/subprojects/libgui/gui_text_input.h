#ifndef GUI_TEXT_INPUT_H
#define GUI_TEXT_INPUT_H

#include "gui_elem.h"
#include "gui_layer.h"
#include "gui_text_view.h"

typedef struct gui_scrollbar gui_scrollbar_t;

typedef struct gui_text_input {
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
    gui_layer_t* content_layer;
    Rect content_layer_frame;
    bool controls_content_layer;
    Color background_color;
    
    // Callbacks
    gui_mouse_entered_cb_t mouse_entered_cb;
    gui_mouse_exited_cb_t mouse_exited_cb;
    gui_mouse_moved_cb_t mouse_moved_cb;
    gui_mouse_dragged_cb_t mouse_dragged_cb;
    gui_mouse_left_click_cb_t left_click_cb;
    gui_key_down_cb_t key_down_cb;
    gui_key_up_cb_t key_up_cb;
    gui_window_resized_cb_t window_resized_cb;
    gui_teardown_cb_t teardown_cb;

    // Private gui_view_t fields
    array_t* subviews;
    gui_view_t* superview;
    uint32_t border_margin;
    uint32_t title_bar_height;
    char* _title;
    Rect _title_inset;
    gui_layer_t* parent_layer;
    gui_view_elem_for_mouse_pos_cb_t elem_for_mouse_pos_cb;
    gui_draw_cb_t _fill_background_cb;

    // Private gui_scroll_view_t fields
    gui_scrollbar_t* scrollbar;
    Size full_content_area_size;

    // Public gui_text_view_t fields
    Point cursor_pos;
    Size font_size;
    Size font_inset;

    // gui_text_input_t fields
    bool _input_carat_visible;
    Color font_color;
} gui_text_input_t;

gui_text_input_t* gui_text_input_alloc(void);
void gui_text_input_init(gui_text_input_t* view, gui_window_t* window, gui_window_resized_cb_t sizer_cb);

void gui_text_input_add_subview(gui_view_t* superview, gui_text_input_t* subview);
void gui_text_input_add_to_window(gui_text_input_t* view, gui_window_t* window);
gui_text_input_t* gui_text_input_create(gui_window_t* window, gui_window_resized_cb_t sizer_cb);

#endif
