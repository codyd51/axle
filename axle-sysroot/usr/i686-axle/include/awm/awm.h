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
typedef struct awm_mouse_moved_msg {
    uint32_t event; // AWM_MOUSE_MOVED
    uint32_t x_pos;
    uint32_t y_pos;
} awm_mouse_moved_msg_t;

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
} awm_mouse_left_click_msg_t;

#define AWM_MOUSE_DRAGGED (1 << 10)
typedef struct awm_mouse_dragged_msg {
    uint32_t event; // AWM_MOUSE_DRAGGED
    uint32_t x_pos;
    uint32_t y_pos;
} awm_mouse_dragged_msg_t;

#define AWM_MOUSE_LEFT_CLICK_ENDED (1 << 11)
typedef struct awm_mouse_left_click_ended_msg {
    uint32_t event; // AWM_MOUSE_LEFT_CLICK_ENDED
    Point click_end_point;
} awm_mouse_left_click_ended_msg_t;

// Sent from preferences to awm
#define AWM_PREFERENCES_UPDATED (1 << 12)

// Sent from a client to awm
#define AWM_UPDATE_WINDOW_TITLE (1 << 13)
typedef struct awm_window_title_msg {
    uint32_t event; // AWM_UPDATE_WINDOW_TITLE
    uint32_t len;
    char title[64];
} awm_window_title_msg_t;

// Sent from a client to awm
#define AWM_CLOSE_WINDOW (1 << 14)
// Sent from awm to a client
#define AWM_CLOSE_WINDOW_REQUEST (1 << 14)

#endif