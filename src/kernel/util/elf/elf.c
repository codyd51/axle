#include "elf.h"
#include <stdint.h>
#include <std/std.h>
#include <std/printf.h>

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
	}
	if (!elf_check_supported(hdr)) {
		printf_err("ELF parser: File not supported");
	}
	printf_info("ELF parser: File passed validation");
	return true;
}

static int elf_load_stage1(elf_header* hdr);
static int elf_load_stage2(elf_header* hdr);

static inline void* elf_load_rel(elf_header* hdr) {
	int result = elf_load_stage1(hdr);
	if (result == ELF_RELOC_ERR) {
		printf_err("ELF loader: Unable to load ELF");
		return NULL;
	}
	result = elf_load_stage2(hdr);
	if (result == ELF_RELOC_ERR) {
		printf_err("ELF loader: Unable to load ELF");
		return NULL;
	}
	//TODO parse program header (if present)
	return (void*)hdr->entry;
}

void* elf_load_file(void* file) {
	elf_header* hdr = (elf_header*)file;
	if (!elf_validate(hdr)) {
		printf_err("ELF loader: File cannot be loaded");
		return NULL;
	}
	switch (hdr->type) {
		case ET_EXEC:
			//TODO implement
			return NULL;
		case ET_REL:
			return elf_load_rel(hdr);
	}
	return NULL;
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

static void* elf_lookup_symbol(const char* name) {
	//TODO implement
	return NULL;
}

static int elf_get_symval(elf_header* hdr, int table, unsigned int idx) {
	if (table == SHN_UNDEF || idx == SHN_UNDEF) return 0;
	elf_s_header* symtab = elf_get_section(hdr, table);

	uint32_t entries = symtab->size / symtab->entsize;
	if (idx >= entries) {
		printf_err("ELF loader: Symbol index %d out of bounds %u", idx, table);
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

static int elf_load_stage1(elf_header* hdr) {
	elf_s_header* shdr = elf_get_s_header(hdr);

	//iterate section headers
	for (int i = 0; i < hdr->shnum; i++) {
		elf_s_header* section = &shdr[i];

		//if section isn't present in file
		if (section->type == SHT_NOBITS) {
			//skip if section is empty
			if (!section->size) continue;

			//should section appear in memory?
			if (section->flags & SHF_ALLOC) {
				//allocate and zero memory
				void* mem = kmalloc(section->size);
				memset(mem, 0, section->size);

				//assign memory offset to section offset
				section->offset = (int)mem - (int)hdr;

				printf_dbg("ELF loader: Allocated memory for section %d (%d)", i, section->size);
			}
		}
	}
	return 0;
}

static int elf_do_reloc(elf_header* hdr, elf_rel* rel, elf_s_header* rel_tab);

static int elf_load_stage2(elf_header* hdr) {
	elf_s_header* shdr = elf_get_s_header(hdr);

	//iterate section headers
	for (int i = 0; i < hdr->shnum; i++) {
		elf_s_header* section = &shdr[i];

		//relocation section?
		if (section->type == SHT_REL) {
			//process each entry in table
			for (int idx = 0; i < section->size / section->entsize; idx++) {
				elf_rel* rel_tab = &((elf_rel*)((int)hdr + section->offset))[idx];
				int result = elf_do_reloc(hdr, rel_tab, section);
				
				if (result == ELF_RELOC_ERR) {
					printf_err("ELF loader: Failed to relocate symbol");
					return ELF_RELOC_ERR;
				}
			}
		}
	}
	return 0;
}

#define DO_386_32(S, A)		((S) + (A))
#define DO_386_PC32(S, A, P) 	((S) + (A) - (P))

static int elf_do_reloc(elf_header* hdr, elf_rel* rel, elf_s_header* rel_tab) {
	elf_s_header* target = elf_get_section(hdr, rel_tab->info);

	int addr = (int)hdr + target->offset;
	int* ref = (int*)(addr + rel->offset);

	//symbol val
	int symval = 0;
	if (ELF_R_SYM(rel->info) != SHN_UNDEF) {
		symval = elf_get_symval(hdr, rel_tab->link, ELF_R_SYM(rel->info));
		if (symval == ELF_RELOC_ERR) return ELF_RELOC_ERR;
	}

	//relocate based on type
	switch (ELF_R_TYPE(rel->info)) {
		case R_386_NONE:
			//no relocation
			break;
		case R_386_32:
			//symbol + offset
			*ref = DO_386_32(symval, *ref);
			break;
		case R_386_PC32:
			//symbol + offset - section offset
			*ref = DO_386_PC32(symval, *ref, (int)ref);
			break;
		default:
			//unsupported relocation type
			printf_err("ELF loader: Unsupported relocation type %d", ELF_R_TYPE(rel->info));
			return ELF_RELOC_ERR;
	}
	return symval;
}
