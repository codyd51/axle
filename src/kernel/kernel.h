#ifndef KERNEL_H
#define KERNEL_H

#include <std/std.h>
#include <kernel/drivers/kb/kb.h>
#include <kernel/drivers/vga/vga.h>

void terminal_putchar(char c);
void terminal_writestring(const char* data);
void terminal_removechar();
void terminal_clear();
void terminal_settextcolor(enum vga_color col); 

#endif
