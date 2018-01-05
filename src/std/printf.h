#ifndef PRINTF_H
#define PRINTF_H

#include <stdarg.h>

//standard printf
int printf(const char* format, ...);
//same as above, but outputs to syslog
//(applies to all _k functions listed here)
int printk(const char* format, ...);

//debug-priority printf
int printf_dbg(const char* format, ...);
int printk_dbg(const char* format, ...);

//info-priority printf
int printf_info(const char* format, ...);
int printk_info(const char* format, ...);

//error-priority printf
int printf_err(const char* format, ...);
int printk_err(const char* format, ...);

int snprintf(char* buffer, unsigned int buffer_len, const char *fmt, ...);

//unimplemented functions
//calling any of these will throw an NotImplemented() assertion
int vprintf();
int putchar();
int sprintf();
int output();
int reset_cursor_pos();

#endif
