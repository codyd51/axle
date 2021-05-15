#include "elf.h"
#include <stdint.h>
#include <kernel/boot_info.h>
#include <std/std.h>
#include <std/printf.h>
#include <std/kheap.h>
#include <kernel/util/paging/paging.h>
#include <kernel/multitasking//tasks/task.h>
#include <kernel/multitasking/tasks/task_small.h>
#include <kernel/util/paging/paging.h>

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
	if (hdr->ident[EI_CLASS] != ELFCLASS32) {
		return false;
	}
	if (hdr->ident[EI_DATA] != ELFDATA2LSB) {
		return false;
	}
	if (hdr->machine != EM_386) {
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
	uint32_t dest_base = seg->vaddr;
	uint32_t dest_limit = dest_base + seg->memsz;

	// Map the segment memory
	uint32_t mem_pages = (seg->memsz + (PAGE_SIZE-1)) & PAGING_PAGE_MASK;
	uint32_t* mem_base = (uint32_t*)seg->vaddr;
	printf("Page-aligned segment size: 0x%08x\n", mem_pages);
	printf("Allocating at 0x%08x\n", seg->vaddr);
	for (uint32_t i = 0; i < mem_pages; i += PAGE_SIZE) {
		// TODO(PT): Add some kind of validation that the page isn't already mapped
		uint32_t* mem_addr = (uint32_t*)(seg->vaddr + i);
		uint32_t frame_addr = vmm_alloc_page_address_usermode(vmm_active_pdir(), mem_addr, true);
		memset(mem_addr, 0, PAGE_SIZE);
	}

	// Copy the file data
	memcpy(mem_base, src_base, seg->filesz);
	
	return true;
}

uint32_t elf_load_small(unsigned char* src) {
	elf_header* hdr = (elf_header*)src;
	uintptr_t phdr_table_addr = (uint32_t)hdr + hdr->phoff;

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

void elf_load_buffer(char* program_name, uint8_t* buf, uint32_t buf_size, char** argv) {
	elf_header* hdr = (elf_header*)buf;
	if (!elf_validate_header(hdr)) {
		printf("validation failed\n");
		return;
	}

	char* string_table = elf_get_string_table(hdr, buf_size);

	uint32_t prog_break = 0;
	uint32_t bss_loc = 0;
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

	uint32_t entry_point = elf_load_small((unsigned char*)buf);
	//printf("ELF prog_break 0x%08x bss_loc 0x%08x\n", prog_break, bss_loc);
	// TODO(PT): Ensure the caller cleans this up?
	//kfree(buf);

	// give user program a 32kb stack
	// TODO(PT): We need to free the stack created by _thread_create
	uint32_t stack_size = PAGING_PAGE_SIZE*2;
	printf("ELF allocating stack with PDir 0x%08x\n", vmm_active_pdir());
	uint32_t stack_bottom = vmm_alloc_continuous_range(vmm_active_pdir(), stack_size, true, 0xd0000000, true);
	printf("[%d] allocated ELF stack at 0x%08x\n", getpid(), stack_bottom);
    uint32_t *stack_top = (uint32_t *)(stack_bottom + stack_size); // point to top of malloc'd stack
	uint32_t* stack_top_orig = stack_top;
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

	if (entry_point) {
		//vas_active_unmap_temp(sizeof(vmm_page_directory_t));
		task_small_t* elf = tasking_get_task_with_pid(getpid());
		// Ensure the task won't be scheduled while modifying ts critical state
		//spinlock_acquire(&elf->priority_lock);
		asm("cli");
		// TODO(PT): We should store the kmalloc()'d stack in the task structure so that we can free() it once the task dies.
		printf("Set elf->machine_state = 0x%08x\n", stack_top);
		elf->machine_state = (task_context_t*)stack_top;
		elf->sbrk_current_break = prog_break;
		elf->bss_segment_addr = bss_loc;
		elf->sbrk_current_page_head = (elf->sbrk_current_break + PAGE_SIZE) & PAGING_PAGE_MASK;
		//printf("strdup program name 0x%08x\n", program_name);
		//printf("strlen %d\n", strlen(program_name));

		//elf->name = strdup(program_name);
		elf->name = strdup("launched_elf");

		printf("[%d] Jump to user-mode with ELF [%s] ip=0x%08x sp=0x%08x\n", elf->id, elf->name, entry_point, elf->machine_state);
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
	//find file size
	fseek(elf, 0, SEEK_END);
	uint32_t binary_size = ftell(elf);
	fseek(elf, 0, SEEK_SET);

	char* filebuf = kmalloc(binary_size);
	for (uint32_t i = 0; i < binary_size; i++) {
		filebuf[i] = fgetc(elf);
	}
	elf_load_buffer(name, filebuf, binary_size, argv);
	// The above should never return
	assert(false, "elf_load_buffer returned execution to loader!");
}
