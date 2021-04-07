#ifndef AWM_H
#define AWM_H

#include <kernel/amc.h>

#define AWM_SERVICE_NAME "com.axle.awm"

// Overload the "send" and "receive" names to be the same command
// When AWM receives it, it will interpret as a request to provide a framebuffer
// When a client receives it, it will interpret as a provided framebuffer
#define AWM_REQUEST_WINDOW_FRAMEBUFFER (1 << 0)
#define AWM_CREATED_WINDOW_FRAMEBUFFER (1 << 0)

#define AWM_WINDOW_REDRAW_READY (1 << 1)

#define AWM_MOUSE_ENTERED (1 << 2)
#define AWM_MOUSE_EXITED (1 << 3)
#define AWM_MOUSE_MOVED (1 << 4)

#define AWM_KEY_DOWN (1 << 5)
#define AWM_KEY_UP (1 << 6)

#define AWM_MOUSE_SCROLLED (1 << 7)
typedef struct awm_mouse_scrolled_msg {
    uint32_t event; // AWM_MOUSE_SCROLLED
    int8_t delta_z;
} awm_mouse_scrolled_msg_t;

#define AWM_WINDOW_RESIZED (1 << 8)
typedef struct awm_window_resized_msg {
    uint32_t event; // AWM_WINDOW_RESIZED
    Size new_size;
} awm_window_resized_msg_t;

#define AWM_MOUSE_LEFT_CLICK (1 << 9)
typedef struct awm_mouse_left_click_msg {
    uint32_t event; // AWM_MOUSE_LEFT_CLICK
    Point click_point;
} awm_mouse_left_click_msg;

#endif