#ifndef KERNEL_H
#define KERNEL_H

#include <std/std.h>
#include <kernel/drivers/kb/kb.h>
#include <kernel/drivers/terminal/terminal.h>
#include <kernel/elf.h>

elf_t* kern_elf();

#endif
