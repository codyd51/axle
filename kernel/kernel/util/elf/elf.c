#include "elf.h"
#include <stdint.h>
#include <kernel/boot_info.h>
#include <std/std.h>
#include <std/printf.h>
#include <std/kheap.h>
#include <kernel/vmm/vmm.h>
#include <kernel/multitasking/tasks/task_small.h>

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
		return false;
	}
	if (hdr->ident[EI_DATA] != ELFDATA2LSB) {
		return false;
	}
	if (hdr->machine != EM_x86_64) {
		return false;
	}
	if (hdr->ident[EI_VERSION] != EV_CURRENT) {
		return false;
	}
	if (hdr->type != ET_REL && hdr->type != ET_EXEC) {
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

bool elf_validate(FILE* file) {
	char buf[sizeof(elf_header)];
	fseek(file, 0, SEEK_SET);
	///fread(&buf, sizeof(elf_header), 1, file);
	for (uint32_t i = 0; i < sizeof(elf_header); i++) {
		buf[i] = fgetc(file);
	}
	elf_header* hdr = (elf_header*)(&buf);

	fseek(file, 0, SEEK_SET);
	return elf_validate_header(hdr);
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

	// Map the segment memory
	uintptr_t page_aligned_size = (seg->memsz + (PAGE_SIZE-1)) & PAGING_PAGE_MASK;
	printf("Page-aligned segment size: 0x%p\n", page_aligned_size);
	printf("Allocating at 0x%p\n", seg->vaddr);
	/*
	for (uint32_t i = 0; i < page_aligned_size; i += PAGE_SIZE) {
		// TODO(PT): Add some kind of validation that the page isn't already mapped
		uintptr_t* mem_addr = (uintptr_t*)(seg->vaddr + i);
		//uintptr_t frame_addr = vmm_alloc_page_address_usermode(vmm_active_pdir(), mem_addr, true);
		vas_alloc
		uintptr_t frame_addr =
		NotImplemented();

		memset(mem_addr, 0, PAGE_SIZE);
	}
	*/
	uintptr_t* base = vas_alloc_range(vas_get_active_state(), seg->vaddr, page_aligned_size, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_USER);
	assert(base == seg->vaddr, "Failed to map program at its requested address");

	// Copy the file data
	memcpy(base, src_base, seg->filesz);
	
	return true;
}

uintptr_t elf_load_small(unsigned char* src) {
	elf_header* hdr = (elf_header*)src;
	uintptr_t phdr_table_addr = (uintptr_t)hdr + hdr->phoff;

	int segcount = hdr->phnum; 
	if (!segcount) return 0;

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
	return 0;
}

char* elf_get_string_table(void* file, uint32_t binary_size) {
	elf_header* hdr = (elf_header*)file;
	char* string_table;
	uint32_t i = 0;
	for (int x = 0; x < hdr->shentsize * hdr->shnum; x += hdr->shentsize) {
		if (hdr->shoff + x > binary_size) {
			printf("ELF: Tried to read beyond the end of the file.\n");
			return NULL;
		}
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

void elf_load_buffer(char* program_name, uint8_t* buf, uint32_t buf_size, char** argv) {
	elf_header* hdr = (elf_header*)buf;
	if (!elf_validate_header(hdr)) {
		printf("validation failed\n");
		return;
	}
	task_small_t* current_task = tasking_get_task_with_pid(getpid());

	char* string_table = elf_get_string_table(hdr, buf_size);
	_record_elf_symbol_table(buf, &current_task->elf_symbol_table);

	uintptr_t prog_break = 0;
	uintptr_t bss_loc = 0;
	for (int x = 0; x < hdr->shentsize * hdr->shnum; x += hdr->shentsize) {
		if (hdr->shoff + x > buf_size) {
			printf("Tried to read beyond the end of the file.\n");
			return;
		}

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
	if (!entry_point) {
		printf("ELF wasn't loadable!\n");
		return;
	}
	//printf("ELF prog_break 0x%08x bss_loc 0x%08x\n", prog_break, bss_loc);
	// TODO(PT): Ensure the caller cleans this up?
	//kfree(buf);

	// Give userspace a 128kb stack
	// TODO(PT): We need to free the stack created by _thread_create
	uint32_t stack_size = PAGE_SIZE * 32;
	printf("ELF allocating stack with PDir 0x%p\n", vas_get_active_state());
	uintptr_t stack_bottom = vas_alloc_range(vas_get_active_state(), 0x7e0000000000, stack_size, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_USER);
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
	printf("Argv 0x%08x\n", argv);
	while (argv[argc] != NULL) {
		argc++;
	}

	if (entry_point) {
		//vas_active_unmap_temp(sizeof(vmm_page_directory_t));
		// Ensure the task won't be scheduled while modifying its critical state
		//spinlock_acquire(&elf->priority_lock);
		asm("cli");
		// TODO(PT): We should store the kmalloc()'d stack in the task structure so that we can free() it once the task dies.
		printf("Set elf->machine_state = 0x%08x\n", stack_top);
		current_task->machine_state = (task_context_t*)stack_top;
		current_task->sbrk_current_break = prog_break;
		current_task->bss_segment_addr = bss_loc;
		current_task->sbrk_current_page_head = (current_task->sbrk_current_break + PAGE_SIZE) & PAGING_PAGE_MASK;

		task_set_name(current_task, "launched_elf");

		printf("[%d] Jump to user-mode with ELF [%s] ip=0x%08x sp=0x%08x\n", current_task->id, current_task->name, entry_point, current_task->machine_state);
		//spinlock_release(&elf->priority_lock);
		user_mode(stack_top, entry_point);

		// binary should have called _exit()
		// if we got to this point, something went catastrophically wrong
		ASSERT(0, "ELF binary returned execution to loader!");
	}
	else {
		printf_err("ELF wasn't loadable!");
		return;
	}
}

void elf_load_file(char* name, FILE* elf, char** argv) {
	Deprecated();
}
