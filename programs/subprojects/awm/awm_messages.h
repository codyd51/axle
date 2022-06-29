#ifndef AWM_MESSAGES_H
#define AWM_MESSAGES_H

#include <kernel/amc.h>
#include <libagx/lib/size.h>
#include <libagx/lib/point.h>
#include <libagx/lib/color.h>

#define AWM_SERVICE_NAME "com.axle.awm"

// Note: awm picks its event values to try to avoid conflicts with 
// programs that send events their own events to/from awm

// Overload the "send" and "receive" names to be the same command
// When AWM receives it, it will interpret as a request to provide a framebuffer
// When a client receives it, it will interpret as a provided framebuffer
#define AWM_CREATE_WINDOW_REQUEST 800
typedef struct awm_create_window_request {
    uint32_t event; // AWM_CREATE_WINDOW_REQUEST
    Size window_size;
} awm_create_window_request_t;

#define AWM_CREATE_WINDOW_RESPONSE 800
typedef struct awm_create_window_response {
    uint32_t event; // AWM_CREATE_WINDOW_RESPONSE
    Size screen_resolution;
    int bytes_per_pixel;
    void* framebuffer;
} awm_create_window_response_t;

#define AWM_WINDOW_REDRAW_READY 801

#define AWM_MOUSE_ENTERED 802
#define AWM_MOUSE_EXITED 803

#define AWM_MOUSE_MOVED 804
typedef struct awm_mouse_moved_msg {
    uint32_t event; // AWM_MOUSE_MOVED
    uint32_t x_pos;
    uint32_t y_pos;
} awm_mouse_moved_msg_t;

#define AWM_KEY_DOWN 805
#define AWM_KEY_UP 806

#define AWM_MOUSE_SCROLLED 807
typedef struct awm_mouse_scrolled_msg {
    uint32_t event; // AWM_MOUSE_SCROLLED
    int8_t delta_z;
} awm_mouse_scrolled_msg_t;

#define AWM_WINDOW_RESIZED 808
typedef struct awm_window_resized_msg {
    uint32_t event; // AWM_WINDOW_RESIZED
    Size new_size;
} awm_window_resized_msg_t;

#define AWM_MOUSE_LEFT_CLICK 809
typedef struct awm_mouse_left_click_msg {
    uint32_t event; // AWM_MOUSE_LEFT_CLICK
    Point click_point;
} awm_mouse_left_click_msg_t;

#define AWM_MOUSE_DRAGGED 810
typedef struct awm_mouse_dragged_msg {
    uint32_t event; // AWM_MOUSE_DRAGGED
    uint32_t x_pos;
    uint32_t y_pos;
} awm_mouse_dragged_msg_t;

#define AWM_MOUSE_LEFT_CLICK_ENDED 811
typedef struct awm_mouse_left_click_ended_msg {
    uint32_t event; // AWM_MOUSE_LEFT_CLICK_ENDED
    Point click_end_point;
} awm_mouse_left_click_ended_msg_t;

// Sent from preferences to awm
#define AWM_PREFERENCES_UPDATED 812

// Sent from a client to awm
#define AWM_UPDATE_WINDOW_TITLE 813
typedef struct awm_window_title_msg {
    uint32_t event; // AWM_UPDATE_WINDOW_TITLE
    uint32_t len;
    char title[64];
} awm_window_title_msg_t;

// Sent from a client to awm
#define AWM_CLOSE_WINDOW 814
// Sent from awm to a client
#define AWM_CLOSE_WINDOW_REQUEST 814

// Sent from preferences to awm
#define AWM_DESKTOP_TRAITS_REQUEST 815
#define AWM_DESKTOP_TRAITS_RESPONSE 815
typedef struct awm_desktop_traits_response {
    uint32_t event;
    Color desktop_gradient_inner_color;
    Color desktop_gradient_outer_color;
} awm_desktop_traits_response_t;

#define AWM_WINDOW_RESIZE_ENDED 816

// Sent from awm to the dock
#define AWM_DOCK_WINDOW_CREATED 817
typedef struct awm_dock_window_created_event {
    uint32_t event;
    uint32_t window_id;
    uint32_t title_len;
    const char title[64];
} awm_dock_window_created_event_t;

#define AWM_DOCK_WINDOW_TITLE_UPDATED 818
typedef struct awm_dock_window_title_updated_event {
    uint32_t event;
    uint32_t window_id;
    uint32_t title_len;
    const char title[64];
} awm_dock_window_title_updated_event_t;

#endif