#ifndef READ_H
#define READ_H

#include <stdint.h>
#include <std/std.h>

uint32_t read(int fd, void* buf, uint32_t count);

#endif
