#ifndef PRINTF_H
#define PRINTF_H

#include <kernel/drivers/terminal/terminal.h>
#include <stdarg.h>

enum {
	TERM_OUTPUT = 0,
	SERIAL_OUTPUT,
};

void output(int dest, char* str);
void outputc(int dest, char ch);

//standard printf
void printf(char* format, ...);
int putchar(int ch);
//same as above, but outputs to syslog
//(applies to all _k functions listed here)
void printk(char* format, ...);

//debug-priority printf
void printf_dbg(char* format, ...);
void printk_dbg(char* format, ...);

//info-priority printf
void printf_info(char* format, ...);
void printk_info(char* format, ...);

//error-priority printf
void printf_err(char* format, ...);
void printk_err(char* format, ...);

void sprintf(char* str, char* format, ...);

#define stdin  0
#define stdout 1
#define stderr 2
//fprintf stub
//axle's current filesystem doesn't support writeable files,
//so fprintf only works with stdout and stderr.
//any other stream causes a critical failure
void fprintf(int stream, char* format, ...);

void reset_cursor_pos();

#endif
