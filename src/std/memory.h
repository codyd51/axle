#ifndef STD_MEMORY_H
#define STD_MEMORY_H

#include "std_base.h"
#include <stddef.h>

__BEGIN_DECLS

STDAPI int memcp(const void*, const void*, size_t);
STDAPI void* memcpy(void* __restrict, const void* __restrict, size_t);
STDAPI void* memmove(void*, const void*, size_t);
STDAPI void* memset(void*, int, size_t);
STDAPI void* calloc(size_t num, size_t size);
STDAPI void* realloc(void* ptr, size_t size);

__END_DECLS

#endif // STD_MEMORY_H
