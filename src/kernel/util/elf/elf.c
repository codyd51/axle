#include "elf.h"
#include <stdint.h>
#include <std/std.h>
#include <std/printf.h>
#include <std/kheap.h>
#include <kernel/util/paging/paging.h>
#include <kernel/util/multitasking/tasks/task.h>
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

bool elf_load_segment(page_directory_t* new_dir, unsigned char* src, elf_phdr* seg) {
	//loadable?
	if (seg->type != PT_LOAD) {
		printf_err("Tried to load non-loadable segment");
		printk_err("Tried to load non-loadable segment");
		return false; 
	}

	unsigned char* src_base = src + seg->offset;
	//figure out range to map this binary to in virtual memory
	uint32_t dest_base = seg->vaddr;
	uint32_t dest_limit = dest_base + seg->memsz;

	printf("dest_base %x dest_limit %x\n", dest_base, dest_limit);
	//alloc enough mem for new task
	for (uint32_t i = dest_base, page_counter = 0; i <= dest_limit; i += PAGE_SIZE, page_counter++) {
		page_t* page = get_page(i, 1, new_dir);
		ASSERT(page, "elf_load_segment couldn't get page in new addrspace at %x\n", i);
		bool got_frame = alloc_frame(page, 0, 0);
		ASSERT(got_frame, "elf_load_segment couldn't alloc frame for page %x\n", i);
		
		char* pagebuf = kmalloc_a(PAGE_SIZE);
		page_t* local_page = get_page((uint32_t)pagebuf, 0, page_dir_current());
		ASSERT(local_page, "couldn't get local_page!");
		int old_frame = local_page->frame;
		local_page->frame = page->frame;
		invlpg(pagebuf);

		//create buffer in current address space,
		//copy data,
		//and then map frame into new address space
		memset(pagebuf, 0, (dest_limit - dest_base));
		//only seg->filesz bytes are garuanteed to be in the file!
		//_not_ memsz
		//any extra bytes between filesz and memsz should be set to 0, which is done above
		//memcpy(dest_base, src_base, seg->filesz);
		memcpy(pagebuf, src_base + (page_counter * PAGE_SIZE), seg->filesz);

		//now that we've copied the data in the local address space, 
		//get the page in local address space, 
		//and copy backing physical frame data to physical frame of
		//page in new address space

		//now that the buffer has been copied, we can safely free the buffer
		local_page->frame = old_frame;
		invlpg(pagebuf);
		kfree(pagebuf);
	}

	// Copy data
	//memset((void*)dest_base, 0, (void*)(dest_limit - dest_base));

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
	printf("ELF .bss mapped @ %x - %x\n", shdr->addr, shdr->addr + shdr->size);
	for (uint32_t i = 0; i <= shdr->size + PAGE_SIZE; i += PAGE_SIZE) {
		page_t* page = get_page(shdr->addr + i, 1, new_dir);
		if (!alloc_frame(page, 0, 1)) {
			printf_err(".bss %x wasn't alloc'd", shdr->addr + i);
		}

		char* pagebuf = kmalloc_a(PAGE_SIZE);
		//zero out .bss
		memset(pagebuf, 0, PAGE_SIZE);

		page_t* local_page = get_page((uint32_t)pagebuf, 1, page_dir_current());
		ASSERT(local_page, "elf_load_segment couldn't find page for pagebuf");

		extern void copy_page_physical(uint32_t page, uint32_t dest);
		copy_page_physical(local_page->frame * PAGE_SIZE, page->frame * PAGE_SIZE);

		//now that the buffer has been copied, we can safely free the buffer
		kfree(pagebuf);
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

	uint32_t new_dir_phys = 0;
	page_directory_t* new_dir = kmalloc_ap(sizeof(page_directory_t), &new_dir_phys);
	memset((uint8_t*)new_dir, 0, sizeof(page_directory_t));
	//get offset of tablesPhysical from start of page_directory_t
	uint32_t offset = (uint32_t)new_dir->tablesPhysical - (uint32_t)new_dir;
	new_dir->physicalAddr = new_dir_phys + offset;

	for (int i = 0; i < 1024; i++) {
		new_dir->tables[i] = page_dir_kern()->tables[i];
		new_dir->tablesPhysical[i] = page_dir_kern()->tablesPhysical[i] & ~4;
	}

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
			alloc_bss(new_dir, shdr, (int*)&prog_break, (int*)&bss_loc);
		}
	}

	uint32_t entry = elf_load_small(new_dir, (unsigned char*)filebuf);
	kfree(filebuf);

	//alloc stack space
	uint32_t stack_addr = 0x10000000;
	//give user program a 128kb stack
	for (int i = 0; i < 32; i++) {
		page_t* stacktop = get_page(stack_addr, 1, new_dir);
		//user, writeable
		alloc_frame(stacktop, 0, 1);
		stack_addr += PAGE_SIZE;
	}
	stack_addr -= PAGE_SIZE;

	//calculate argc count
	int argc = 0;
	while (argv[argc] != NULL) {
		argc++;
	}

	if (entry) {
		become_first_responder();

		kernel_begin_critical();

		task_t* elf = task_with_pid(getpid());
		elf->prog_break = prog_break;
		elf->bss_loc = bss_loc;
		elf->name = strdup(name);
		elf->esp = elf->ebp = stack_addr;

		elf->eip = entry;
		elf->page_dir = new_dir;

		void goto_pid(int id, bool x);
		goto_pid(elf->id, false);

		/*

		int(*elf_main)(int, char**) = (int(*)(int, char**))entry;
		become_first_responder();

		//calculate argc count
		int argc = 0;
		char* tmp = argv[argc];
		while (argv[argc] != NULL) {
			argc++;
		}

		//jump to ELF entry point!
		int ret = elf_main(argc, argv);
		*/

		//binary should have called _exit()
		//if we got to this point, something went catastrophically wrong
		ASSERT(0, "ELF binary returned execution to loader!");
	}
	else {
		printf_err("ELF wasn't loadable!");
		return;
	}
}

