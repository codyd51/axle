#include "elf.h"

bool elf_check_magic(elf_header* hdr) {
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

bool elf_check_supported(elf_header* hdr) {
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
}

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
		return;
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
	return (char*)hdr + elf_get_section(hdr, hdr->shstrndx)->sh_offset;
}

static inline char* elf_lookup_string(elf_header* hdr, int offset) {
	char* strtab = elf_str_table(hdr);
	if (!strtab) return NULL;
	return strtab + offset;
}
