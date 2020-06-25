#include "elf.h"
#include <std/printf.h>
#include <std/string.h>
#include <kernel/assert.h>

void elf_from_multiboot(struct multiboot_info* mb, elf_t* elf) {
	multiboot_elf_section_header_table_t elf_sec = mb->u.elf_sec;
	printf_info("Parsing multiboot header and kernel symbols at 0x%08x", elf_sec);
	elf_section_header_t* sh = (elf_section_header_t*)elf_sec.addr;

	uint32_t shstrtab = sh[elf_sec.shndx].addr;
	for (uint32_t i = 0; i < elf_sec.num; i++) {
		const char* name = (const char*)(shstrtab + sh[i].name);
		printf_info("elf sec %s", name);
		if (!strcmp(name, ".strtab")) {
			elf->strtab = (const char*)sh[i].addr;
			elf->strtabsz = sh[i].size;
			printf_info("elf found strtab at 0x%08x - 0x%08x", elf->strtab, sh[i].addr + elf->strtabsz);
		}
		if (!strcmp(name, ".symtab")) {
			elf->symtab = (elf_symbol_t*)sh[i].addr;
			elf->symtabsz = sh[i].size;
			printf_info("elf found symtab at 0x%08x - 0x%08x, size %d", elf->symtab, sh[i].addr + elf->symtabsz);
		}
	}
}

const char* elf_sym_lookup(elf_t* elf, uint32_t addr) {
	asm("cli");
	for (uint32_t i = 0; i < (elf->symtabsz / sizeof(elf_symbol_t)); i++) {
		const char* name = (const char*)((uint32_t)elf->strtab + elf->symtab[i].name);
		//function type is 0x2
		if (ELF32_ST_TYPE(elf->symtab[i].info) != 0x2) {
			continue;
		}

		//check if addr is bounded by this function
		if ((addr >= elf->symtab[i].value) &&
			(addr < (elf->symtab[i].value + elf->symtab[i].size))) {
            asm("sti");
			return name;
		}
	}
	asm("sti");
	return "?";
}
