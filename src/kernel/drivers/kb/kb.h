#ifndef KB_DRIVER_H
#define KB_DRIVER_H

#include <std/std.h>
#include "keymap.h"

__BEGIN_DECLS

#define CONTROL		0x1
#define ALT			0x2
#define ALTGR		0x4
#define LSHIFT		0x8
#define RSHIFT		0x10
#define CAPSLOCK	0x20
#define SCROLLLOCK	0x40
#define NUMLOCK		0x80

#define RELEASED_MASK 0x80

//non-blocking getchar()
//returns NULL if no pending keys
char kgetch();
//blocks until character is recieved
char getchar();
//check if there is a pending keypress
bool haskey();
//return mask of modifier keys
key_status_t kb_modifiers();

//install PS/2 keyboard driver
void kb_install();
//swap layout to interpret incoming scancodes
void switch_layout(keymap_t* layout);
//get current layout
keymap_t* kb_layout();

__END_DECLS

#endif
