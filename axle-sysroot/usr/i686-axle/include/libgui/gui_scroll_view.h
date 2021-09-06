#ifndef GUI_SCROLL_VIEW_H
#define GUI_SCROLL_VIEW_H


#include "gui_elem.h"
#include "gui_view.h"
#include "gui_layer.h"
#include "gui_scrollbar.h"

typedef struct gui_scroll_view {
    gui_view_t base;
    // Private gui_scroll_view_t fields
    gui_scrollbar_t* scrollbar;
    Size full_content_area_size;
} gui_scroll_view_t;

gui_scroll_view_t* gui_scroll_view_alloc(void);
void gui_scroll_view_alloc_dynamic_fields(gui_scroll_view_t* view);
void gui_scroll_view_init(gui_scroll_view_t* view, gui_window_t* window, gui_window_resized_cb_t sizer_cb);
void gui_scroll_view_add_subview(gui_view_t* superview, gui_scroll_view_t* subview);
void gui_scroll_view_add_to_window(gui_scroll_view_t* view, gui_window_t* window);

// Combines -alloc, -init, and -add_to_window
gui_scroll_view_t* gui_scroll_view_create(gui_window_t* window, gui_window_resized_cb_t sizer_cb);

// Friend function for subclasses
// TODO(PT): Store the callback chain in the structure so each subclass can invoke the parent?
void _gui_scroll_view_draw(gui_scroll_view_t* sv, bool is_active);

#endif