#ifndef LIBGUI_H
#define LIBGUI_H

#include <stdlibadd/array.h>

#include <kernel/amc.h>

#include <agx/lib/ca_layer.h>
#include <agx/lib/text_box.h>
#include <agx/lib/hash_map.h>

typedef union gui_elem gui_elem_t;

typedef struct gui_window gui_window_t;
typedef void (*gui_interrupt_cb_t)(gui_window_t* window, uint32_t int_no);
typedef void (*gui_amc_message_cb_t)(gui_window_t* window, amc_message_t* msg);

typedef struct gui_window {
    array_t* _interrupt_cbs;
    gui_amc_message_cb_t _amc_handler;

    Size size;
    ca_layer* layer;
	array_t* text_inputs;
	array_t* text_views;

    // The GUI element the mouse is currently hovered over
    gui_elem_t* hover_elem;
    array_t* all_gui_elems;
} gui_window_t;

typedef struct text_input text_input_t;
typedef void(*text_input_text_entry_cb_t)(text_input_t* input, char ch);

typedef void(*gui_mouse_entered_cb_t)(gui_elem_t* gui_elem);
typedef void(*gui_mouse_exited_cb_t)(gui_elem_t* gui_elem);
typedef void(*gui_mouse_moved_cb_t)(gui_elem_t* gui_elem, Point mouse_pos);
typedef void(*gui_mouse_dragged_cb_t)(gui_elem_t* gui_elem, Point mouse_pos);
typedef void(*gui_mouse_left_click_cb_t)(gui_elem_t* gui_elem, Point mouse_pos);
typedef void(*gui_mouse_left_click_ended_cb_t)(gui_elem_t* gui_elem, Point mouse_pos);
typedef void(*gui_mouse_scrolled_cb_t)(gui_elem_t* gui_elem, int8_t delta_z);

typedef void(*gui_draw_cb_t)(gui_elem_t* gui_elem, bool is_active);
typedef void(*_priv_gui_window_resized_cb_t)(gui_elem_t* gui_elem, Size new_size);

typedef Rect(*gui_window_resized_cb_t)(gui_elem_t* gui_elem, Size new_size);

typedef struct text_input {
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

    // Public and shared in union
	Rect frame;
    gui_window_resized_cb_t sizer_cb;

    // Public fields
    gui_window_t* window;
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
    gui_draw_cb_t _priv_draw_cb;
    _priv_gui_window_resized_cb_t _priv_window_resized_cb;

    // Public and shared in union
	Rect frame;
    gui_window_resized_cb_t sizer_cb;

    // Public fields
    gui_window_t* window;
	text_box_t* text_box;
    
    // Callbacks
    gui_mouse_entered_cb_t mouse_entered_cb;
    gui_mouse_exited_cb_t mouse_exited_cb;
    gui_mouse_moved_cb_t mouse_moved_cb;
    uint32_t text_box_margin;

    // Private fields
    gui_scrollbar_t* scrollbar;
} text_view_t;

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
    gui_draw_cb_t _priv_draw_cb;
    _priv_gui_window_resized_cb_t _priv_window_resized_cb;

    // Public and shared in union
	Rect frame;
    gui_window_resized_cb_t sizer_cb;

    // Public fields
    gui_window_t* window;
    gui_elem_t* parent;
    float scroll_percent;
    bool hidden;

    // Callbacks
    gui_scrollbar_updated_cb_t scroll_position_updated_cb;

    // Private fields
    ca_layer* layer;
    bool in_left_click;
} gui_scrollbar_t;

typedef union gui_elem {
    text_input_t ti;
    text_view_t tv;
    gui_scrollbar_t sb;
} gui_elem_t;

gui_window_t* gui_window_create(uint32_t width, uint32_t height);

text_input_t* gui_text_input_create(gui_window_t* window, Rect frame, Color background_color, gui_window_resized_cb_t sizer_cb);
void gui_text_input_set_prompt(text_input_t* ti, char* prompt);
void gui_text_input_clear(text_input_t* text_input);
void gui_text_input_destroy(text_input_t* text_input);

text_view_t* gui_text_view_create(gui_window_t* window, Rect frame, Color background_color, gui_window_resized_cb_t sizer_cb);
void gui_text_view_clear(text_view_t* text_view);
void gui_text_view_destroy(text_view_t* text_view);

gui_scrollbar_t* gui_scrollbar_create(gui_window_t* window, gui_elem_t* parent, Rect frame, gui_window_resized_cb_t sizer_cb);
void gui_scrollbar_destroy(gui_scrollbar_t* scrollbar);

void gui_enter_event_loop(gui_window_t* window);

void gui_add_interrupt_handler(gui_window_t* window, uint32_t int_no, gui_interrupt_cb_t cb);
void gui_add_message_handler(gui_window_t* window, gui_amc_message_cb_t cb);

#endif