#include "common.h"

//page aligned
u32int kmalloc_a(u32int sz);
//returns physical address
u32int kmalloc_p(u32int sz, u32int* phys);
//page aligned and returns physical address
u32int kmalloc_ap(u32int sz, u32int* phys);
//normal kmalloc
u32int kmalloc(u32int sz);
