#ifndef PRINTF_H
#define PRINTF_H

#include <kernel/drivers/terminal/terminal.h>
#include <stdarg.h>

//standard printf
void printf(char* format, ...);
//debug-priority printf
void printf_dbg(char* format, ...);
//info-priority printf
void printf_info(char* format, ...);
//error-priority printf
void printf_err(const char* format, ...);
void vprintf_err(const char* format, va_list ap);

void sprintf(char* str, char* format, ...);

#endif
