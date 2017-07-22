#ifndef KERNEL_H
#define KERNEL_H

#include <std/std.h>
#include <kernel/drivers/kb/kb.h>
#include <kernel/drivers/terminal/terminal.h>
#include <kernel/elf.h>
#include "multiboot.h"

elf_t* kern_elf();
multiboot* kernel_multiboot_ptr();
//returns system RAM available, in kilobytes, as reported by GRUB
uint32_t system_mem();
//fill @p start and @p end with the beginning and end of initrd,
//respectively, in physical memory. 
//This is before initrd is remapped by GRUB; this is the address GRUB loaded the module
void initrd_loc(uint32_t* start, uint32_t* end);

#endif
