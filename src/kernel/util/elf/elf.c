#include "elf.h"
#include <stdint.h>
#include <std/std.h>
#include <std/printf.h>
#include <std/kheap.h>
#include <kernel/util/vfs/fs.h>
#include <kernel/util/paging/paging.h>
#include <kernel/util/multitasking/tasks/task.h>

static bool elf_check_magic(elf_header* hdr) {
	if (!hdr) return false;

	if (hdr->ident[EI_MAG0] != ELFMAG0) {
		printf_err("ELF parser: EI_MAG0 (%d) incorrect", hdr->ident[EI_MAG0]);
		return false;
	}
	if (hdr->ident[EI_MAG1] != ELFMAG1) {
		printf_err("ELF parser: EI_MAG1 (%d) incorrect", hdr->ident[EI_MAG1]);
		return false;
	}
	if (hdr->ident[EI_MAG2] != ELFMAG2) {
		printf_err("ELF parser: EI_MAG2 (%d) incorrect", hdr->ident[EI_MAG2]);
		return false;
	}
	if (hdr->ident[EI_MAG3] != ELFMAG3) {
		printf_err("ELF parser: EI_MAG3 (%d) incorrect", hdr->ident[EI_MAG3]);
		return false;
	}
	return true;
}

static bool elf_check_supported(elf_header* hdr) {
	if (hdr->ident[EI_CLASS] != ELFCLASS32) {
		printf_err("ELF parser: Unsupported file class");
		return false;
	}
	if (hdr->ident[EI_DATA] != ELFDATA2LSB) {
		printf_err("ELF parser: Unsupported byte order");
		return false;
	}
	if (hdr->machine != EM_386) {
		printf_err("ELF parser: Unsupported target");
		return false;
	}
	if (hdr->ident[EI_VERSION] != EV_CURRENT) {
		printf_err("ELF parser: Unsupported version");
		return false;
	}
	if (hdr->type != ET_REL && hdr->type != ET_EXEC) {
		printf_err("ELF parser: Unsupported file type");
		return false;
	}
	return true;
}

bool elf_validate(elf_header* hdr) {
	if (!elf_check_magic(hdr)) {
		printf_err("ELF parser: Invalid ELF magic");
		return false;
	}
	if (!elf_check_supported(hdr)) {
		printf_err("ELF parser: File not supported");
		return false;
	}
	printf_info("ELF parser: File passed validation");
	return true;
}

void test_elf() {
	char* filename = "test.elf";
	printf("Loading ELF %s\n", filename);
	FILE* elf = fopen(filename, "r");
	if (!elf) {
		printf_err("ELF couldn't find file!");
		printk_err("ELF couldn't find file!");
		return;
	}

	//find file size
	fseek(elf, 0, SEEK_END);
	uint32_t size = ftell(elf);
	fseek(elf, 0, SEEK_SET);

	char* filebuf = kmalloc(size);
	for (int i = 0; i < size; i++) {
		filebuf[i] = fgetc(elf);
	}
	elf_load_file(filebuf, size);
}

bool elf_load_segment(unsigned char* src, elf_phdr* seg) {
	printf("ELF loading segment type %d (%x) ", seg->type, seg);
	printk("ELF loading segment type %d (%x) ", seg->type, seg);

	//loadable?
	if (seg->type != PT_LOAD) {
		printf_err("Tried to load non-loadable segment");
		printk_err("Tried to load non-loadable segment");
		return false; 
	}

	//unsigned char* src_base = &src[seg->offset];
	unsigned char* src_base = src + seg->offset;
	//figure out range to map this binary to in virtual memory
	unsigned char* dest_base = (unsigned char*)seg->vaddr;

	//uint32_t dest_limit = ((uint32_t) dest_base + seg->memsz + 0x1000) & ~0xFFF;
	unsigned char* dest_limit = (uintptr_t)(dest_base + seg->memsz + 0x1000) & 0xFFFFF000;

	printf("@ [%x to %x]\n", dest_base, dest_limit);
	//printf_info("Mapping seg from %x to %x", dest_base, dest_limit);
	//printk_info("Mapping seg from %x to %x", dest_base, dest_limit);

	//alloc enough mem for new task
	for (uint32_t i = dest_base; i < dest_limit; i += 0x1000) {
#include <kernel/util/paging/paging.h>
		extern page_directory_t* current_directory;
		//printf("alloc'ing page @ virt %x\n", i);
		//printk("alloc'ing page @ virt %x\n", i);
		page_t* page = get_page(i, 1, current_directory);
		if (page) {
			if (!alloc_frame(page, 1, 1)) {
				//printf_err("ELF: alloc_frame failed");
				//while (1) {}
			}
		}
	}
	//      p_alloc(&t->map, i, PF_USER);

	// Copy data
	//printf_info("copy [%x, %x] to [%x, %x]", src_base, src_base + seg->memsz, dest_base, dest_limit);
	//printk_info("copy [%x, %x] to [%x, %x]", src_base, src_base + seg->memsz, dest_base, dest_limit);
	memcpy(dest_base, src_base, seg->memsz);

	// Set proper flags (i.e. remove write flag if needed)
	/*
	   if (seg->p_flags & PF_W) {
	   i = ((u32int) dest_base) & ~0xFFF;
	   for (; i < dest_limit; i+= 0x1000)
	   page_set(&t->map, i, page_fmt(page_get(&t->map, i), PF_USER | PF_PRES));
	   }
	   */
	return true;
}

uint32_t elf_load_small(unsigned char* src) {
	//draw_boot_background();

	elf_header* hdr = (elf_header*)src;
	//elf_phdr* phdr = (elf_phdr*)&src[hdr->phoff];
	elf_phdr* phdr_table = (elf_phdr*)((uint32_t)hdr + hdr->phoff);
	uintptr_t phdr_table_addr = (uint32_t)hdr + hdr->phoff;

	int segcount = hdr->phnum; 
	if (!segcount) return 0;

	printf_info("ELF has %d segments", segcount);
	printk_info("ELF has %d segments", segcount);

	bool found_loadable_seg = false;
	//load each segment
	for (int i = 0; i < segcount; i++) {
		//elf_phdr* segment = phdr_table[i];
		elf_phdr* segment = (elf_phdr*)(phdr_table_addr + (i * hdr->phentsize));
		//if (elf_load_segment(src, &hdr[i])) {
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

void* elf_load_file(void* file, uint32_t binary_size) {
	elf_header* hdr = (elf_header*)file;
	char* string_table = elf_get_string_table(hdr, binary_size);

	uint32_t prog_break = 0;
	for (uint32_t x = 0; x < hdr->shentsize * hdr->shnum; x += hdr->shentsize) {
		if (hdr->shoff + x > binary_size) {
			printf("Tried to read beyond the end of the file.\n");
			return NULL;
		}

		elf_s_header* shdr = (elf_s_header*)((uintptr_t)file + (hdr->shoff + x));
		char* section_name = (char*)((uintptr_t)string_table + shdr->name);

		//alloc memory for .bss segment
		if (!strcmp(section_name, ".bss")) {
			uintptr_t page_aligned = shdr->size + 
									 (0x1000 - 
									 (shdr->size % 0x1000));
			printf(".bss @ [%x to %x]\n", shdr->addr, shdr->addr + page_aligned);
			for (int i = 0; i <= page_aligned; i += 0x1000) {
				extern page_directory_t* current_directory;
				alloc_frame(get_page(shdr->addr + i, 1, current_directory), 0, 0);
			}
			//set program break to .bss segment
			prog_break = shdr->addr + shdr->offset + shdr->size;
		}
	}

	uint32_t entry = elf_load_small(file);
	printf_info("ELF entry point @ %x, valid? %s", entry, (entry) ? "yes, jumping..." : "no");
	if (entry) {
		if (!fork("ELF Program")) {
			task_t* elf = task_with_pid(getpid());
			elf->prog_break = prog_break;

			elf->load_addr = 0x08054000;

			int(*elf_main)(void) = (int(*)(void))entry;
			become_first_responder();

			int ret = elf_main();

			printf("ELF returned with status %x\n", ret);
			//TODO replace w/ exit() sysall
			_kill();
		}
	}
	else {
		printf_err("ELF wasn't loadable!");
		printk_err("ELF wasn't loadable!");
		return;
	}

}

static inline elf_s_header* elf_get_s_header(elf_header* hdr) {
	return (elf_s_header*)((int)hdr + hdr->shoff);
}

static inline elf_s_header* elf_get_section(elf_header* hdr, int idx) {
	return &elf_get_s_header(hdr)[idx];
}

static inline char* elf_str_table(elf_header* hdr) {
	if (hdr->shstrndx == SHN_UNDEF) return NULL;
	return (char*)hdr + elf_get_section(hdr, hdr->shstrndx)->offset;
}

static inline char* elf_lookup_string(elf_header* hdr, int offset) {
	char* strtab = elf_str_table(hdr);
	if (!strtab) return NULL;
	return strtab + offset;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static void* elf_lookup_symbol(const char* name) {
	//TODO implement
	return NULL;
}
#pragma GCC diagnostic pop

static int elf_get_symval(elf_header* hdr, int table, unsigned int idx) {
	if (table == SHN_UNDEF || idx == SHN_UNDEF) return 0;
	elf_s_header* symtab = elf_get_section(hdr, table);

	uint32_t entries = symtab->size / symtab->entsize;
	if (idx >= entries) {
		printf_err("ELF loader: Symbol index %d out of bounds %u", idx, table);
		printk_err("ELF loader: Symbol index %d out of bounds %u", idx, table);
		return ELF_RELOC_ERR;
	}

	int addr = (int)hdr + symtab->offset;
	elf_sym_tab* symbol = &((elf_sym_tab*)addr)[idx];

	if (symbol->shndx == SHN_UNDEF) {
		//external symbol!
		//lookup value
		elf_s_header* str_tab = elf_get_section(hdr, symtab->link);
		const char* name = (const char*)hdr + str_tab->offset + symbol->name;

		void* target = elf_lookup_symbol(name);
		if (!target) {
			//external symbol not found
			if (ELF32_ST_BIND(symbol->info) & STB_WEAK) {
				//weak symbol initialized to 0
				return 0;
			}
			else {
				printf_err("ELF loader: Undefined external symbol: %s", name);
				return ELF_RELOC_ERR;
			}
		}
		else {
			return (int)target;
		}
	}
	else if (symbol->shndx == SHN_ABS) {
		//absolute symbol
		return symbol->value;
	}
	else {
		//internally defined symbol
		elf_s_header* target = elf_get_section(hdr, symbol->shndx);
		return (int)hdr + symbol->value + target->offset;
	}
}

