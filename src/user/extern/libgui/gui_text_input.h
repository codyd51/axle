#ifndef GUI_TEXT_INPUT_H
#define GUI_TEXT_INPUT_H

#include "gui_elem.h"

typedef struct text_input text_input_t;
typedef void(*text_input_text_entry_cb_t)(text_input_t* input, char ch);

typedef struct text_input {
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
	uint8_t* text;
	uint32_t len;

    // Callbacks
    gui_mouse_entered_cb_t mouse_entered_cb;
    gui_mouse_exited_cb_t mouse_exited_cb;
    gui_mouse_moved_cb_t mouse_moved_cb;
    text_input_text_entry_cb_t text_entry_cb; 

    // Private
    char* prompt;
	uint32_t max_len;
    uint32_t text_box_margin;
    bool indicator_on;
    uint32_t next_indicator_flip_ms;
} text_input_t;

text_input_t* gui_text_input_create(gui_window_t* window, Rect frame, Color background_color, gui_window_resized_cb_t sizer_cb);
void gui_text_input_set_prompt(text_input_t* ti, char* prompt);
void gui_text_input_clear(text_input_t* text_input);
void gui_text_input_destroy(text_input_t* text_input);

#endif