#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include <stdbool.h>
#include <std/std.h>

void serial_init();

void serial_putchar(char c);
void serial_writestring(char* str);

#endif
