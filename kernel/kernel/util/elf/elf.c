#include "elf.h"
#include <stdint.h>
#include <kernel/boot_info.h>
#include <std/std.h>
#include <std/printf.h>
#include <std/kheap.h>
#include <kernel/vmm/vmm.h>
#include <kernel/multitasking/tasks/task_small.h>
#include <kernel/assert.h>

static bool elf_check_magic(elf_header* hdr) {
	if (!hdr) return false;

	if (hdr->ident[EI_MAG0] != ELFMAG0) {
		return false;
	}
	if (hdr->ident[EI_MAG1] != ELFMAG1) {
		return false;
	}
	if (hdr->ident[EI_MAG2] != ELFMAG2) {
		return false;
	}
	if (hdr->ident[EI_MAG3] != ELFMAG3) {
		return false;
	}
	return true;
}

static bool elf_check_supported(elf_header* hdr) {
	//__x86_64
	if (hdr->ident[EI_CLASS] != ELFCLASS64) {
		printf("Not 64 bit\n");
		return false;
	}
	if (hdr->ident[EI_DATA] != ELFDATA2LSB) {
		printf("Not littend\n");
		return false;
	}
	if (hdr->machine != EM_x86_64) {
		printf("Not x86_64\n");
		return false;
	}
	if (hdr->ident[EI_VERSION] != EV_CURRENT) {
		printf("Not current version\n");
		return false;
	}
	if (hdr->type != ET_REL && hdr->type != ET_EXEC) {
		// TODO(PT): Only executable is supported now?
		printf("Not relocatable/executable\n");
		return false;
	}
	return true;
}

bool elf_validate_header(elf_header* hdr) {
	if (!elf_check_magic(hdr)) {
		printf("Magic failed\n");
		return false;
	}
	if (!elf_check_supported(hdr)) {
		printf("supported failed\n");
		return false;
	}
	return true;
}

bool elf_load_segment(unsigned char* src, elf_phdr* seg) {
	if (seg->type != PT_LOAD) {
		printf("Tried to load non-loadable segment\n");
		return false; 
	}

	unsigned char* src_base = src + seg->offset;
	// Check where to map the binary memory within the address space
	uintptr_t dest_base = seg->vaddr;
	uintptr_t dest_limit = dest_base + seg->memsz;

	//printf("Page-aligned segment size: 0x%p\n", page_aligned_size);
	printf("pid %d Allocating at 0x%p\n", getpid(), seg->vaddr);
	// Floor the vmaddr to a page boundary
	uintptr_t vm_page_base = seg->vaddr & PAGING_PAGE_MASK;
	// Account for the extra bytes we may have just added via the flooring above
	uintptr_t page_aligned_size = (seg->memsz + (seg->vaddr - vm_page_base));
	page_aligned_size = (page_aligned_size + (PAGE_SIZE - 1)) & PAGING_PAGE_MASK;

	// Map the segment memory
	//pmm_debug_on();
	uintptr_t* base = vas_alloc_range(vas_get_active_state(), vm_page_base, page_aligned_size, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_USER);
	//pmm_debug_off();
	assert(base == vm_page_base, "Failed to map program at its requested address");

	// Zero-out any unused bits...
	memset(base, 0, page_aligned_size);
	// Copy the file data, and ignore our floor() from earlier
	memcpy(seg->vaddr, src_base, seg->filesz);

	return true;
}

uintptr_t elf_load_small(unsigned char* src) {
	elf_header* hdr = (elf_header*)src;
	uintptr_t phdr_table_addr = (uintptr_t)hdr + hdr->phoff;

	int segcount = hdr->phnum; 
	task_assert(segcount > 0, "No loadable segments in ELF", NULL);

	printf("[%d] Loading %d ELF segments\n", getpid(), segcount);
	bool found_loadable_seg = false;
	for (int i = 0; i < segcount; i++) {
		elf_phdr* segment = (elf_phdr*)(phdr_table_addr + (i * hdr->phentsize));
		if (elf_load_segment(src, segment)) {
			found_loadable_seg = true;
		}
	}

	//return entry point
	if (found_loadable_seg) {
		return hdr->entry;
	}

	task_assert(segcount > 0, "Failed to find an entry point", NULL);
	return 0;
}

char* elf_get_string_table(void* file, uint32_t binary_size) {
	elf_header* hdr = (elf_header*)file;
	char* string_table;
	uint32_t i = 0;
	for (int x = 0; x < hdr->shentsize * hdr->shnum; x += hdr->shentsize) {
		task_assert(hdr->shoff + x<= binary_size, "Tried to read beyond the end of an ELF", NULL);

		elf_s_header* shdr = (elf_s_header*)(file + (hdr->shoff + x));
		if (i == hdr->shstrndx) {
			string_table = (char *)(file + shdr->offset);
			return string_table;
		}
		i++;
	}
	return NULL;
}

static void _record_elf_symbol_table(void* buf, elf_t* elf) {
	elf_header* hdr = (elf_header*)buf;
	elf_s_header* shstrtab = (uint8_t*)((uint8_t*)buf + hdr->shoff + (hdr->shstrndx * hdr->shentsize));
	uint8_t* strtab = (uint8_t*)buf + shstrtab->offset;

	for (int i = 0; i < hdr->shentsize * hdr->shnum; i += hdr->shentsize) {
		elf_s_header* shdr = (elf_s_header*)(buf + (hdr->shoff + i));
		if (shdr->type == 0) continue;
		uint8_t* name = strtab + shdr->name;
		if (!strcmp(name, ".strtab")) {
			elf->strtab = (const char*)((uint8_t*)buf + shdr->offset);
			elf->strtabsz = shdr->size;
		}
		if (!strcmp(name, ".symtab")) {
			elf->symtab = (const char*)((uint8_t*)buf + shdr->offset);
			elf->symtabsz = shdr->size;
		}
	}
}

// TODO(PT): Ensure this is in the sysroot
//#include <sys/axle/syscalls.h>

void elf_load_buffer(char* program_name, char** argv, uint8_t* buf, uint32_t buf_size, bool free_buffer) {
	printf("ELF loading %s for PID %d\n", program_name, getpid());
	elf_header* hdr = (elf_header*)buf;

	// Note that since we don't call this through a syscall, we have no register state available
	task_assert(elf_validate_header(hdr), "ELF header validation failed", NULL);

	task_small_t* current_task = tasking_get_task_with_pid(getpid());

	char* string_table = elf_get_string_table(hdr, buf_size);
	_record_elf_symbol_table(buf, &current_task->elf_symbol_table);

	uintptr_t prog_break = 0;
	uintptr_t bss_loc = 0;
	for (int x = 0; x < hdr->shentsize * hdr->shnum; x += hdr->shentsize) {
		task_assert(hdr->shoff + x<= buf_size, "Tried to read beyond the end of an ELF", NULL);

		elf_s_header* shdr = (elf_s_header*)((uintptr_t)buf + (hdr->shoff + x));
		char* section_name = (char*)((uintptr_t)string_table + shdr->name);

		//alloc memory for .bss segment
		if (!strcmp(section_name, ".bss")) {
			//set program break to .bss segment
			prog_break = shdr->addr + shdr->size;
			bss_loc = shdr->addr;
		}
	}

	uintptr_t entry_point = elf_load_small((unsigned char*)buf);
	if (free_buffer) {
		printf("[ELF] Freeing buffer 0x%p\n", buf);
		kfree(buf);
	}

	task_assert(entry_point != 0, "Failed to find an ELF entry point", NULL);

	//printf("ELF prog_break 0x%08x bss_loc 0x%08x\n", prog_break, bss_loc);

	// Give userspace a 128kb stack
	// TODO(PT): We need to free the stack created by _thread_create
	char msg_buf[512];
	snprintf(msg_buf, 512, "ELF alloc stack for %s\n", program_name);
	//draw_string_oneshot(msg_buf);

	printf("ELF allocating stack with PDir 0x%p\n", vas_get_active_state());
	uint32_t stack_size = USER_MODE_STACK_SIZE;
	uintptr_t stack_bottom = vas_alloc_range(
		vas_get_active_state(), 
		USER_MODE_STACK_BOTTOM, 
		stack_size, 
		VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_USER
	);
	printf("[%d] allocated ELF stack at 0x%08x\n", getpid(), stack_bottom);
    uintptr_t *stack_top = (uintptr_t *)(stack_bottom + stack_size); // point to top of malloc'd stack
	uintptr_t* stack_top_orig = stack_top;
	printf("[%d] Set ESP to 0x%08x\n", getpid(), stack_top);
    *(--stack_top)= 0xaa;   //address of task's entry point
    *(--stack_top)= 0xbb;   //address of task's entry point
    *(--stack_top)= 0x0;   // alignment
    *(--stack_top)= entry_point;   //address of task's entry point
    *(--stack_top)= 0x1;             //esi
    *(--stack_top)= 0x2;             //edi
    *(--stack_top)   = 0x3;             //ebp
    *(--stack_top)   = 0x4;             //ebp
    *(--stack_top)   = 0x5;             //ebp

	//calculate argc count
	int argc = 0;
	while (argv[argc] != NULL) {
		argc++;
	}

	//vas_active_unmap_temp(sizeof(vmm_page_directory_t));
	// Ensure the task won't be scheduled while modifying its critical state
	//spinlock_acquire(&elf->priority_lock);
	asm("cli");
	// TODO(PT): We should store the kmalloc()'d stack in the task structure so that we can free() it once the task dies.
	//printf("Set elf->machine_state = 0x%08x\n", stack_top);
	current_task->machine_state = (task_context_t*)stack_top;
	current_task->sbrk_current_break = prog_break;
	current_task->bss_segment_addr = bss_loc;
	current_task->sbrk_current_page_head = (current_task->sbrk_current_break + PAGE_SIZE) & PAGING_PAGE_MASK;

	task_set_name(current_task, "launched_elf");

	printf("[%d] Jump to user-mode with ELF [%s] ip=0x%08x sp=0x%08x\n", current_task->id, current_task->name, entry_point, current_task->machine_state);
	snprintf(msg_buf, 512, "Jump to user mode for %s\n", program_name);
	//draw_string_oneshot(msg_buf);
	//spinlock_release(&elf->priority_lock);
	user_mode(stack_top, entry_point);

	// Binary should terminate via _exit()
	task_assert(false, "ELF returned execution to loader", NULL);
}
