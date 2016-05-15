#include "std.h"
#include "kb.h"
#include "vga.h"

void terminal_putchar(char c);
void terminal_writestring(const char* data);
void terminal_removechar();
void terminal_clear();
void terminal_settextcolor(enum vga_color col); 
