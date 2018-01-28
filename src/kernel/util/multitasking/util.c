#include "util.h"
#include <std/std.h>
#include <std/memory.h>
#include <kernel/util/paging/paging.h>
#include <kernel/vmm/vmm.h>

extern uint32_t initial_esp;

void move_stack(void* new_stack_start, uint32_t size) {
	//allocate space for new stack
	printf_dbg("allocating stack space at %x of size %x", new_stack_start, size);
    vmm_map_region(vmm_active_pdir(), new_stack_start, size);

	//flush TLB by reading and writing page directory address again
	printf_dbg("flushing TLB");
	uint32_t pd_addr;
	asm volatile("mov %%cr3, %0" : "=r" (pd_addr));
	asm volatile("mov %0, %%cr3" : : "r" (pd_addr));

	//old ESP and EBP
	uint32_t old_sp;
	asm volatile("mov %%esp, %0" : "=r" (old_sp));
	uint32_t old_bp;
	asm volatile("mov %%ebp, %0" : "=r" (old_bp));

	//offset to add to old stack addresses to get new stack address
	uint32_t offset = (uint32_t)new_stack_start - initial_esp;

	//new esp and ebp
	uint32_t new_sp = old_sp + offset;
	uint32_t new_bp = old_bp + offset;
	printf_dbg("new ESP %x, new EBP %x", new_sp, new_bp);

	//copy stack!
	printf("copying stack data!\n");
	memcpy((void*)new_sp, (void*)old_sp, initial_esp - old_sp);

	//backtrace through original stack, copying new values into new stack
	for (uint32_t i = (uint32_t)new_stack_start; i > (uint32_t)new_stack_start - size; i -= sizeof(uint32_t)) {
		uint32_t tmp = *(uint32_t*)i;
		//if value of tmp is inside range of old stack,
		//assume it's a base pointer and remap it
		//TODO keep in mind this will remap ANY value in this range,
		//whether it's a base pointer or not
		if ((old_sp < tmp) && (tmp < initial_esp)) {
			tmp = tmp + offset;
			uint32_t* tmp2 = (uint32_t*)i;
			*tmp2 = tmp;
		}
	}

	//change stacks
	asm volatile("mov %0, %%esp" : : "r" (new_sp));
	asm volatile("mov %0, %%ebp" : : "r" (new_bp));
}
