#ifndef AWM_MESSAGES_H
#define AWM_MESSAGES_H

#include <kernel/amc.h>

#define AWM_SERVICE_NAME "com.axle.awm"

// Note: awm picks its event values to try to avoid conflicts with 
// programs that send events their own events to/from awm

// Overload the "send" and "receive" names to be the same command
// When AWM receives it, it will interpret as a request to provide a framebuffer
// When a client receives it, it will interpret as a provided framebuffer
#define AWM_REQUEST_WINDOW_FRAMEBUFFER 800
#define AWM_CREATED_WINDOW_FRAMEBUFFER 800

#define AWM_WINDOW_REDRAW_READY 801

#define AWM_MOUSE_ENTERED 802
#define AWM_MOUSE_EXITED 803

#define AWM_MOUSE_MOVED 804
typedef struct awm_mouse_moved_msg {
    uint32_t event; // Expects AWM_MOUSE_MOVED
    uint32_t x_pos;
    uint32_t y_pos;
} awm_mouse_moved_msg_t;

#define AWM_KEY_DOWN 805
#define AWM_KEY_UP 806

#define AWM_MOUSE_SCROLLED 807
typedef struct awm_mouse_scrolled_msg {
    uint32_t event; // Expects AWM_MOUSE_SCROLLED
    int8_t delta_z;
} awm_mouse_scrolled_msg_t;

#define AWM_WINDOW_RESIZED 808
typedef struct awm_window_resized_msg {
    uint32_t event; // Expects AWM_WINDOW_RESIZED
    Size new_size;
} awm_window_resized_msg_t;

#define AWM_MOUSE_LEFT_CLICK 809
typedef struct awm_mouse_left_click_msg {
    uint32_t event; // Expects AWM_MOUSE_LEFT_CLICK
    Point click_point;
} awm_mouse_left_click_msg_t;

#define AWM_MOUSE_DRAGGED 810
typedef struct awm_mouse_dragged_msg {
    uint32_t event; // Expects AWM_MOUSE_DRAGGED
    uint32_t x_pos;
    uint32_t y_pos;
} awm_mouse_dragged_msg_t;

#define AWM_MOUSE_LEFT_CLICK_ENDED 811
typedef struct awm_mouse_left_click_ended_msg {
    uint32_t event; // Expects AWM_MOUSE_LEFT_CLICK_ENDED
    Point click_end_point;
} awm_mouse_left_click_ended_msg_t;

// Sent from preferences to awm
#define AWM_PREFERENCES_UPDATED 812

// Sent from a client to awm
#define AWM_UPDATE_WINDOW_TITLE 813
typedef struct awm_window_title_msg {
    uint32_t event; // Expects AWM_UPDATE_WINDOW_TITLE
    uint32_t len;
    char title[64];
} awm_window_title_msg_t;

// Sent from a client to awm
#define AWM_CLOSE_WINDOW 814
// Sent from awm to a client
#define AWM_CLOSE_WINDOW_REQUEST 814

#endif