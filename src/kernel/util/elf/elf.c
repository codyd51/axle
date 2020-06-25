#include "elf.h"
#include <stdint.h>
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
		return false;
	}
	if (!elf_check_supported(hdr)) {
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

bool elf_load_segment(vmm_page_directory_t* new_dir_virt, unsigned char* src, elf_phdr* seg) {
	printf("***** elf_load_segment 0x%08x 0x%08x %d\n", src, seg, seg->type);
	//loadable?
	if (seg->type != PT_LOAD) {
		printf_err("Tried to load non-loadable segment");
		return false; 
	}

	unsigned char* src_base = src + seg->offset;
	//figure out range to map this binary to in virtual memory
	uint32_t dest_base = seg->vaddr;
	uint32_t dest_limit = dest_base + seg->memsz;

	vmm_page_directory_t* virt_new_pd = new_dir_virt;
	//alloc enough mem for new task

	uint32_t remaining_bytes = seg->filesz;
	for (uint32_t i = dest_base, page_counter = 0; /*i <= dest_limit || */remaining_bytes > 0; i += PAGE_SIZE, page_counter++) {
		uint32_t src_off = src_base + (page_counter * PAGE_SIZE);
		// uint32_t page = vmm_alloc_page_for_frame(new_dir, i, true);
		uint32_t frame_addr = vas_virt_alloc_page_address(virt_new_pd, i, true);
		printf("ELF seg map page %d, frame 0x%08x virt 0x%08x\n", page_counter, frame_addr, i);
		uint32_t local_page = vas_active_map_phys_range(frame_addr, PAGING_FRAME_SIZE);
		//printf("local page 0x%08x\n", local_page);
		memset(local_page, 0, PAGING_PAGE_SIZE);
		uint32_t bytes_to_copy = MIN(remaining_bytes, PAGING_PAGE_SIZE);
		remaining_bytes -= bytes_to_copy;
		printf("ELF copy 0x%08x bytes from 0x%08x to 0x%08x\n", bytes_to_copy, src_off, frame_addr);
		memcpy(local_page, src_off, bytes_to_copy);
		vmm_unmap_range(vmm_active_pdir(), local_page, PAGING_PAGE_SIZE);
	}
	
	return true;
}

uint32_t elf_load_small(page_directory_t* new_dir, unsigned char* src) {
	elf_header* hdr = (elf_header*)src;
	uintptr_t phdr_table_addr = (uint32_t)hdr + hdr->phoff;

	int segcount = hdr->phnum; 
	if (!segcount) return 0;

	bool found_loadable_seg = false;
	//load each segment
	for (int i = 0; i < segcount; i++) {
		elf_phdr* segment = (elf_phdr*)(phdr_table_addr + (i * hdr->phentsize));
		if (elf_load_segment(new_dir, src, segment)) {
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


//map pages for bss segment pointed to by shdr
//stores program break (end of .bss segment) in prog_break
//stored start of .bss segment in bss_loc
static void alloc_bss(page_directory_t* new_dir, elf_s_header* shdr, int* prog_break, int* bss_loc) {
	printf("ELF .bss mapped @ 0x%08x - 0x%08x\n", shdr->addr, shdr->addr + shdr->size);
	for (uint32_t i = 0; i <= shdr->size + PAGE_SIZE; i += PAGE_SIZE) {
		uint32_t virt_addr = shdr->addr + i;
		uint32_t frame_addr = vas_virt_alloc_page_address(new_dir, virt_addr, true);
		uint32_t local_page = vas_active_map_phys_range(frame_addr, PAGING_FRAME_SIZE);
		memset(local_page, 0, PAGING_PAGE_SIZE);
		vmm_unmap_range(vmm_active_pdir(), local_page, PAGING_PAGE_SIZE);
	}
	//set program break to .bss segment
	*prog_break = shdr->addr + shdr->size;
	*bss_loc = shdr->addr;
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

	elf_header* hdr = (elf_header*)filebuf;
	if (!elf_validate_header(hdr)) {
		return;
	}

	// TODO(PT): Free the VAS we're using up to now
	vmm_page_directory_t* phys_new_pd = vmm_clone_active_pdir();
    vmm_page_directory_t* virt_new_pd = (vmm_page_directory_t*)vas_active_map_temp(phys_new_pd, sizeof(vmm_page_directory_t));

	char* string_table = elf_get_string_table(hdr, binary_size);

	uint32_t prog_break = 0;
	uint32_t bss_loc = 0;
	for (int x = 0; x < hdr->shentsize * hdr->shnum; x += hdr->shentsize) {
		if (hdr->shoff + x > binary_size) {
			printf("Tried to read beyond the end of the file.\n");
			return;
		}

		elf_s_header* shdr = (elf_s_header*)((uintptr_t)filebuf + (hdr->shoff + x));
		char* section_name = (char*)((uintptr_t)string_table + shdr->name);

		//alloc memory for .bss segment
		if (!strcmp(section_name, ".bss")) {
			alloc_bss(virt_new_pd, shdr, (int*)&prog_break, (int*)&bss_loc);
		}
	}

	uint32_t entry_point = elf_load_small(virt_new_pd, (unsigned char*)filebuf);
	printf("ELF prog_break 0x%08x bss_loc 0x%08x\n", prog_break, bss_loc);
	kfree(filebuf);

	// give user program a 32kb stack
	uint32_t stack_size = PAGING_PAGE_SIZE * 8;
	uint32_t stack_bottom = vas_virt_alloc_continuous_range(virt_new_pd, stack_size, true);
	printf("allocated ELF stack at 0x%08x\n", stack_bottom);
    uint32_t *stack_top = (uint32_t *)(stack_bottom + stack_size - 0x4); // point to top of malloc'd stack
    *(stack_top--) = entry_point;   //address of task's entry point
    *(stack_top--) = 0;             //eax
    *(stack_top--) = 0;             //ebx
    *(stack_top--) = 0;             //esi
    *(stack_top--) = 0;             //edi
    *(stack_top)   = 0;             //ebp

	//calculate argc count
	int argc = 0;
	printf("ARGV 0x%08x\n", argv);
	while (argv[argc] != NULL) {
		printf("ELF found argv %s\n", argv[argc]);
		argc++;
	}

	if (entry_point) {
		kernel_begin_critical();
		//become_first_responder();
		//vas_active_unmap_temp(sizeof(vmm_page_directory_t));

		task_small_t* elf = tasking_get_task_with_pid(getpid());
		kernel_begin_critical();
		elf->vmm = phys_new_pd;
		elf->machine_state = (task_context_t*)stack_top;
		elf->sbrk_current_break = prog_break;
		elf->bss_segment_addr = bss_loc;
		elf->name = strdup(name);

		printf("Jumping to entry point of ELF [%s (%d)] @ 0x%08x\n", elf->name, elf->id, entry_point);

		void tasking_goto_task(task_small_t* new_task);
        vmm_load_pdir(phys_new_pd, false);
		void (*elf_start)(int, char**) = (void(*)(int, char**))entry_point;
		elf_start(argc, argv);

		//binary should have called _exit()
		//if we got to this point, something went catastrophically wrong
		ASSERT(0, "ELF binary returned execution to loader!");
	}
	else {
		printf_err("ELF wasn't loadable!");
		return;
	}
}

