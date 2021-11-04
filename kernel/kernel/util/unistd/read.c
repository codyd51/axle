#include "read.h"
#include <kernel/drivers/kb/kb.h>
#include <kernel/util/vfs/fs.h>

#include <gfx/lib/gfx.h>

int stdin_read(void* task, int UNUSED(fd), void* buf, int count) {
	Deprecated();
}

uint32_t read(int fd, void* buf, uint32_t count) {
	Deprecated();
}
