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
	for (int i = 0; i < sizeof(elf_header); i++) {
		buf[i] = fgetc(file);
	}
	elf_header* hdr = (elf_header*)(&buf);

	fseek(file, 0, SEEK_SET);
	return elf_validate_header(hdr);
}

bool elf_load_segment(unsigned char* src, elf_phdr* seg) {
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

	//alloc enough mem for new task
	for (uint32_t i = dest_base; i <= dest_limit; i += 0x1000) {
		page_directory_t* current_directory = page_dir_current();
		page_t* page = get_page(i, 1, current_directory);

		if (page) {
			alloc_frame(page, 1, 1);
		}
	}

	// Copy data
	memset((void*)dest_base, 0, (void*)(dest_limit - dest_base));
	//only seg->filesz bytes are garuanteed to be in the file!
	//_not_ memsz
	//any extra bytes between filesz and memsz should be set to 0, which is done above
	memcpy(dest_base, src_base, seg->filesz);

	return true;
}

uint32_t elf_load_small(unsigned char* src) {
	//draw_boot_background();

	elf_header* hdr = (elf_header*)src;
	elf_phdr* phdr_table = (elf_phdr*)((uint32_t)hdr + hdr->phoff);
	uintptr_t phdr_table_addr = (uint32_t)hdr + hdr->phoff;

	int segcount = hdr->phnum; 
	if (!segcount) return 0;

	bool found_loadable_seg = false;
	//load each segment
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
	for (uint32_t x = 0; x < hdr->shentsize * hdr->shnum; x += hdr->shentsize) {
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
}


//map pages for bss segment pointed to by shdr
//stores program break (end of .bss segment) in prog_break
//stored start of .bss segment in bss_loc
static void alloc_bss(elf_s_header* shdr, int* prog_break, int* bss_loc) {
	//printf("ELF .bss mapped @ %x - %x\n", shdr->addr, shdr->addr + shdr->size);
	for (int i = 0; i <= shdr->size + 0x1000; i += 0x1000) {
		extern page_directory_t* current_directory;
		page_t* page = get_page(shdr->addr + i, 1, current_directory);
		if (!alloc_frame(page, 1, 1)) {
			//printf_err(".bss %x wasn't alloc'd", shdr->addr + i);
		}
	}

	//zero out .bss
	char* buf = (char*)shdr->addr;
	memset(buf, 0, shdr->size);

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
	for (int i = 0; i < binary_size; i++) {
		filebuf[i] = fgetc(elf);
	}

	elf_header* hdr = (elf_header*)filebuf;
	if (!elf_validate_header(hdr)) {
		return;
	}

	char* string_table = elf_get_string_table(hdr, binary_size);

	uint32_t prog_break = 0;
	uint32_t bss_loc = 0;
	for (uint32_t x = 0; x < hdr->shentsize * hdr->shnum; x += hdr->shentsize) {
		if (hdr->shoff + x > binary_size) {
			printf("Tried to read beyond the end of the file.\n");
			return;
		}

		elf_s_header* shdr = (elf_s_header*)((uintptr_t)filebuf + (hdr->shoff + x));
		char* section_name = (char*)((uintptr_t)string_table + shdr->name);

		//alloc memory for .bss segment
		if (!strcmp(section_name, ".bss")) {
			alloc_bss(shdr, &prog_break, &bss_loc);
		}
	}

	uint32_t entry = elf_load_small(filebuf);
	kfree(filebuf);

	if (entry) {
		task_t* elf = task_with_pid(getpid());
		elf->prog_break = prog_break;
		elf->bss_loc = bss_loc;
		elf->name = strdup(name);

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

		//binary should have called _exit()
		//if we got to this point, something went catastrophically wrong
		ASSERT(0, "ELF binary returned execution to loader!");
	}
	else {
		printf_err("ELF wasn't loadable!");
		return;
	}
}

