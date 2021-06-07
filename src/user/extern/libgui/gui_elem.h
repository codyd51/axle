#ifndef GUI_ELEM_H
#define GUI_ELEM_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <agx/lib/ca_layer.h>
#include <agx/lib/hash_map.h>
#include <agx/lib/shapes.h>

// From libport

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a <= _b ? _a : _b; })

#define GUI_TYPE_BASE           0x00
#define GUI_TYPE_SCROLLBAR      0x01
#define GUI_TYPE_TEXT_INPUT     0x02
#define GUI_TYPE_TEXT_VIEW      0x03
#define GUI_TYPE_VIEW           0x04
#define GUI_TYPE_SLIDER         0x05
#define GUI_TYPE_BUTTON         0x06

typedef union gui_elem gui_elem_t;
typedef struct gui_window gui_window_t;
typedef struct gui_application gui_application_t;

typedef void(*gui_mouse_entered_cb_t)(gui_elem_t* gui_elem);
typedef void(*gui_mouse_exited_cb_t)(gui_elem_t* gui_elem);
typedef void(*gui_mouse_moved_cb_t)(gui_elem_t* gui_elem, Point mouse_pos);
typedef void(*gui_mouse_dragged_cb_t)(gui_elem_t* gui_elem, Point mouse_pos);
typedef void(*gui_mouse_left_click_cb_t)(gui_elem_t* gui_elem, Point mouse_pos);
typedef void(*gui_mouse_left_click_ended_cb_t)(gui_elem_t* gui_elem, Point mouse_pos);
typedef void(*gui_mouse_scrolled_cb_t)(gui_elem_t* gui_elem, int8_t delta_z);
typedef void(*gui_key_down_cb_t)(gui_elem_t* gui_elem, uint32_t ch);
typedef void(*gui_key_up_cb_t)(gui_elem_t* gui_elem, uint32_t ch);

typedef void(*gui_draw_cb_t)(gui_elem_t* gui_elem, bool is_active);
typedef void(*_priv_gui_window_resized_cb_t)(gui_elem_t* gui_elem, Size new_size);

typedef Rect(*gui_window_resized_cb_t)(gui_elem_t* gui_elem, Size new_size);

typedef void(*gui_teardown_cb_t)(gui_elem_t* gui_elem);

typedef struct gui_elem_base {
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
} gui_elem_base_t;

#endif
