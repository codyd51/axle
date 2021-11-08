#include "elf.h"
#include <std/printf.h>
#include <std/string.h>
#include <kernel/assert.h>

void elf_from_multiboot(struct multiboot_info* mb, elf_t* elf) {
	multiboot_elf_section_header_table_t elf_sec = mb->u.elf_sec;
	printf_info("Parsing multiboot kernel symbols at 0x%08x", elf_sec);

	// A list of section headers placed sequentially after each other
	elf_section_header_t* section_headers = (elf_section_header_t*)elf_sec.addr;
	const uint8_t* section_header_string_table = (const uint8_t*)section_headers[elf_sec.shndx].addr;

	for (uint32_t i = 0; i < elf_sec.num; i++) {
		elf_section_header_t section_header = section_headers[i];
		const char* name = (const char*)(section_header_string_table + section_header.name);
		if (!strcmp(name, ".strtab")) {
			elf->strtab = (const char*)section_header.addr;
			elf->strtabsz = section_header.size;
			printf("Kernel ELF string table: 0x%08x - 0x%08x\n", elf->strtab, elf->strtab + elf->strtabsz);
		}
		if (!strcmp(name, ".symtab")) {
			elf->symtab = (elf_symbol_t*)section_header.addr;
			elf->symtabsz = section_header.size;
			printf("Kernel ELF symbol table: 0x%08x - 0x%08x\n", elf->symtab, elf->symtabsz);
		}
	}
}

const char* elf_sym_lookup(elf_t* elf, uintptr_t addr) {
	for (uint32_t i = 0; i < (elf->symtabsz / sizeof(elf_symbol_t)); i++) {
		elf_symbol_t sym = elf->symtab[i];

		const char* name = elf->strtab + sym.name;

		// Function type is 0x2
		if (ELF32_ST_TYPE(sym.info) != 0x2) {
			continue;
		}

		// Check if the provided address is within this function
		if (addr >= sym.value && addr < (sym.value + sym.size)) {
			return name;
		}
	}
	return NULL;
}
