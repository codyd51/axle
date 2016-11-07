#ifndef STD_KHEAP_H
#define STD_KHEAP_H

#include "std_base.h"
#include "array_o.h"
#include <stdint.h>

__BEGIN_DECLS

#define HEAP_START	0xD0000000
#define HEAP_END	0xFFBFF000

typedef struct heap_header {
	struct heap_header* prev;
	struct heap_header* next;
	uint32_t allocated : 1;
	uint32_t size : 31;
} heap_header_t;

void heap_init();

//returns pointer to chunk of memory of minimum size 'size'
void* kmalloc(uint32_t size);
//takes chunk of memory allocated with kmalloc,
//and returns it to pool of usable memory
void kfree(void* p);

__END_DECLS

#endif // STD_KHEAP_H
