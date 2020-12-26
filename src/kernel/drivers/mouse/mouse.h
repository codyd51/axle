#ifndef MOUSE_H
#define MOUSE_H

#include <gfx/lib/shapes.h>
#include <stdbool.h>

// TODO(PT): use this!
typedef struct mouse_button_state {
    bool left_down : 1;
    bool right_down : 1;
    bool middle_down : 1;
} mouse_button_state_t;

// PS/2 controller calls this to do extra setup for the PS/2 mouse device
void ps2_mouse_enable(void);
// Kernel calls this to launch the mouse driver
void ps2_mouse_driver_launch(void);

//return current mouse coordinates, bounded by VESA 0x118 resolution
Point mouse_point();

//returns current button states in bitmask
//0th bit is left button state
//1st bit is right button state
//2nd bit is middle button state
uint8_t mouse_events();

//blocks running task until mouse event is recieved
void mouse_event_wait();

void mouse_reset_cursorpos();

#endif
