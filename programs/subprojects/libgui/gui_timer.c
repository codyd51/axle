#include <stdio.h>

#include "gui_timer.h"
#include "libgui.h"
#include "utils.h"

void gui_timer_start(uint32_t duration, gui_timer_cb_t timer_cb, void* invoke_ctx) {
    gui_timer_t* t = calloc(1, sizeof(gui_timer_t));
    t->start_time = ms_since_boot();
    t->duration = duration;
    t->fires_after = t->start_time + duration;
    t->invoke_cb = timer_cb;
    t->invoke_ctx = invoke_ctx;
    array_insert(gui_get_application()->timers, t);
}

void gui_dispatch_ready_timers(gui_application_t* app) {
    uintptr_t now = ms_since_boot();
    for (int32_t i = 0; i < app->timers->size; i++) {
        gui_timer_t* t = array_lookup(app->timers, i);
        if (t->fires_after <= now) {
            //uint32_t late_by = now - (t->start_time + t->duration);
            //printf("Dispatching %dms timer at %d, late by %dms\n", t->duration, now, late_by);
            t->invoke_cb(t->invoke_ctx);
        }
    }
    if (app->timers->size > 0) {
        for (int32_t i = app->timers->size - 1; i >= 0; i--) {
            gui_timer_t* t = array_lookup(app->timers, i);
            if (t->fires_after <= now) {
                array_remove(app->timers, i);
                free(t);
            }
        }
    }
}
