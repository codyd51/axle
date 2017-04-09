#ifndef AXLE_OS_H
#define AXLE_OS_H

#include <kernel/kernel.h>
#include "multiboot.h"
#include <stdarg.h>
#include <std/common.h>
#include <std/kheap.h>
#include <std/printf.h>
#include <user/shell/shell.h>
#include <user/xserv/xserv.h>
#include <gfx/lib/gfx.h>
#include <gfx/lib/view.h>
#include <gfx/font/font.h>
#include <kernel/util/syscall/syscall.h>
#include <kernel/util/syscall/sysfuncs.h>
#include <kernel/util/paging/descriptor_tables.h>
#include <kernel/util/paging/paging.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/util/multitasking/tasks/record.h>
#include <kernel/util/mutex/mutex.h>
#include <kernel/util/vfs/initrd.h>
#include <kernel/drivers/rtc/clock.h>
#include <kernel/drivers/pit/pit.h>
#include <kernel/drivers/mouse/mouse.h>
#include <kernel/drivers/vesa/vesa.h>
#include <kernel/drivers/pci/pci_detect.h>
#include <kernel/drivers/serial/serial.h>
#include <std/klog.h>
#include <tests/test.h>
#include <kernel/elf.h>
#include <kernel/util/yxml/yxml.h>
#include <kernel/util/plistman/plistman.h>

#endif
