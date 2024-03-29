#ifndef LIBGUI_H
#define LIBGUI_H

#include <libutils/array.h>

#include <kernel/amc.h>
// For KEY_IDENT_UP_ARROW, etc
#include <drivers/kb/kb_driver_messages.h>

#include "gui_elem.h"
#include "gui_scrollbar.h"
#include "gui_view.h"
#include "gui_slider.h"
#include "gui_button.h"
#include "gui_timer.h"
#include "gui_scroll_view.h"
#include "gui_text_view.h"
#include "gui_text_input.h"

typedef struct gui_application gui_application_t;
typedef void (*gui_interrupt_cb_t)(uint32_t int_no);

typedef void (*gui_amc_message_cb_t)(amc_message_t* msg);
typedef void (*gui_amc_message_batch_cb_t)(array_t* messages);

typedef struct gui_window {
    Size size;
    gui_layer_t* layer;
	array_t* views;

    // The GUI element the mouse is currently hovered over
    gui_elem_t* hover_elem;
    array_t* all_gui_elems;
} gui_window_t;

typedef union gui_elem {
    gui_elem_base_t base;
    gui_scrollbar_t sb;
    gui_view_t v;
    gui_slider_t sl;
    gui_button_t b;
} gui_elem_t;

typedef struct gui_application {
    array_t* windows;
    array_t* timers;

    array_t* _interrupt_cbs;

    gui_amc_message_cb_t _amc_handler;
    gui_amc_message_batch_cb_t _amc_batch_handler;
} gui_application_t;

gui_application_t* gui_application_create(void);
gui_window_t* gui_window_create(char* window_title, uint32_t width, uint32_t height);
void gui_set_window_title(char* window_title);

void gui_enter_event_loop(void);
void gui_run_event_loop_pass(bool prevent_blocking, bool* did_exit);

void gui_add_interrupt_handler(uint32_t int_no, gui_interrupt_cb_t cb);

void gui_add_message_handler(gui_amc_message_cb_t cb);
void gui_add_message_batch_handler(gui_amc_message_batch_cb_t cb);

Size _gui_screen_resolution(void);

gui_application_t* gui_get_application(void);

#endif