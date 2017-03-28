#include "elf.h"

void elf_from_multiboot(multiboot* mb, elf_t* elf) {
	elf_section_header_t* sh = (elf_section_header_t*)mb->addr;

	uint32_t shstrtab = sh[mb->shndx].addr;
	printf("sh %x shstrab %x\n", sh, shstrtab);
	for (int i = 0; i < mb->num; i++) {
		const char* name = (const char*)(shstrtab + sh[i].name);
		if (!strcmp(name, ".strtab")) {
			printf("found strtab\n");
			elf->strtab = (const char*)sh[i].addr;
			elf->strtabsz = sh[i].size;
		}
		if (!strcmp(name, ".symtab")) {
			printf("found symtab\n");
			elf->symtab = (elf_symbol_t*)sh[i].addr;
			elf->symtabsz = sh[i].size;
		}
	}
}

const char* elf_sym_lookup(elf_t* elf, uint32_t addr) {
	for (int i = 0; i < (elf->symtabsz / sizeof(elf_symbol_t)); i++) {
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
