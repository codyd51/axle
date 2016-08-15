#ifndef SYSFUNCS_H
#define SYSFUNCS_H

#include "syscall.h"

//installs common syscalls into syscall table
void create_sysfuncs();

//Standard terminal driver puts
DECL_SYSCALL1(terminal_writestring, const char*);

//Standard terminal driver putc
DECL_SYSCALL1(terminal_putchar, char);

//Yeilds current process's running state to a different process
//Typically invoked if process is blocked by I/O, or sleeping
DECL_SYSCALL0(yield);

#endif
