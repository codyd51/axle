#include "elf.h"
#include <std/printf.h>
#include <std/string.h>
#include <kernel/assert.h>

/*
void elf_from_multiboot(struct multiboot_info* mb, elf_t* elf) {
	printf_info("Parsing multiboot header and kernel symbols");
	elf_section_header_t* sh = (elf_section_header_t*)mb->addr;

	uint32_t shstrtab = sh[mb->shndx].addr;
	for (uint32_t i = 0; i < mb->num; i++) {
		const char* name = (const char*)(shstrtab + sh[i].name);
		if (!strcmp(name, ".strtab")) {
			elf->strtab = (const char*)sh[i].addr;
			elf->strtabsz = sh[i].size;
		}
		if (!strcmp(name, ".symtab")) {
			elf->symtab = (elf_symbol_t*)sh[i].addr;
			elf->symtabsz = sh[i].size;
		}
	}
}

const char* elf_sym_lookup(elf_t* elf, uint32_t addr) {
	for (uint32_t i = 0; i < (elf->symtabsz / sizeof(elf_symbol_t)); i++) {
		const char* name = (const char*)((uint32_t)elf->strtab + elf->symtab[i].name);
		//function type is 0x2
		if (ELF32_ST_TYPE(elf->symtab[i].info) != 0x2) {
			continue;
		}

		//check if addr is bounded by this function
		if ((addr >= elf->symtab[i].value) &&
			(addr < (elf->symtab[i].value + elf->symtab[i].size))) {
			return name;
		}
	}
	return "?";
}
*/

void elf_from_multiboot(struct multiboot_info* mb, elf_t* elf) {
	NotImplemented();
}

const char* elf_symb_lookup(elf_t* elf, uint32_t addr) {
	NotImplemented();
}
