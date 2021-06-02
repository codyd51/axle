#ifndef GUI_TIMER_H
#define GUI_TIMER_H

#include <stdint.h>
#include "gui_elem.h"

typedef void(*gui_timer_cb_t)(void* ctx);

typedef struct gui_timer {
    uint32_t start_time;
    uint32_t duration;
    uint32_t fires_after;
    gui_timer_cb_t invoke_cb;
    void* invoke_ctx;
} gui_timer_t;

void gui_timer_start(uint32_t duration, gui_timer_cb_t timer_cb, void* invoke_ctx);

// Friend function for main event loop
void gui_dispatch_ready_timers(gui_application_t* app);

#endif