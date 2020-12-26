#ifndef KB_DRIVER_H
#define KB_DRIVER_H

#include <std/std.h>

__BEGIN_DECLS

//non-blocking getchar()
//returns NULL if no pending keys
char kgetch();
//blocks until character is recieved
char getchar();
//check if there is a pending keypress
bool haskey();
//return mask of modifier keys
char kb_modifiers();

// Initialize PS/2 keyboard driver
void ps2_keyboard_enable();
void ps2_keyboard_driver_launch();
//swap layout to interpret incoming scancodes
void switch_layout(void* layout);
//get current layout
void* kb_layout();

__END_DECLS

#endif
