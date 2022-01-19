#ifndef SYSFUNCS_H
#define SYSFUNCS_H

#include "syscall.h"
#include <kernel/util/amc/amc.h>
#include <kernel/util/adi/adi.h>

//installs common syscalls into syscall table
void create_sysfuncs();

#endif
