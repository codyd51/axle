#ifndef KB_DRIVER_H
#define KB_DRIVER_H

#include <std/std.h>

__BEGIN_DECLS

// "SSC" means "Scan Code Set" here, i.e. the set of bytes corresponding to
// physical keyboard keys.
#define KBD_SSC_CMD 0xF0
#define KBD_SSC_GET 0x00
#define KBD_SSC_2 0x02
#define KBD_SSC_3 0x03

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
void ps2_keyboard_enable(void);
void ps2_keyboard_driver_launch();
//swap layout to interpret incoming scancodes
void switch_layout(void* layout);
//get current layout
void* kb_layout();

__END_DECLS

#endif
