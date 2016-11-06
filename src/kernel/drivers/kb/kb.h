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

char kgetch();
char getchar();
bool haskey();
void kb_install();
void switch_layout(keymap_t* layout);

__END_DECLS

#endif
