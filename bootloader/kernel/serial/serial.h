#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include <stdbool.h>

void serial_init();

void serial_putchar(char c);
void serial_puts(char* str);

#endif
