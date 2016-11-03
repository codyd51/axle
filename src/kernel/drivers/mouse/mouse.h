#ifndef MOUSE_H
#define MOUSE_H

#include <gfx/lib/shapes.h>

//install mouse driver
void mouse_install();

//return current mouse coordinates, bounded by VESA 0x118 resolution
Coordinate mouse_point();

//returns current button states in bitmask
//0th bit is left button state
//1st bit is right button state
//2nd bit is middle button state
uint8_t mouse_events();

//blocks running task until mouse event is recieved
void mouse_event_wait();

#endif
