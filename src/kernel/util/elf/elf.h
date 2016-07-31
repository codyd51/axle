#ifndef ELF_H
#define ELF_H

#include <stdint.h>

#define ELF_IDENT_LENGTH 16
typedef struct {
	uint8_t		ident[ELF_IDENT_LENGTH];
	uint16_t	type;
	uint16_t 	machine;
	uint32_t 	version;
	uint32_t 	entry;
	uint32_t 	phoff;
	uint32_t 	shoff;
	uint32_t 	flags;
	uint16_t 	ehsize;
	uint16_t 	phentsize;
	uint16_t 	phnum;
	uint16_t 	shentsize;
	uint16_t 	shnum;
	uint16_t 	shstrndx;
} elf_header;

typdef struct {
	uint32_t 	sh_name;
	uint32_t 	sh_type;
	uint32_t 	sh_flags;
	uint32_t 	sh_addr;
	uint32_t 	sh_offset;
	uint32_t 	sh_size;
	uint32_t 	sh_link;
	uint32_t 	sh_info;
	uint32_t 	sh_addralign;
	uint32_t 	sh_entsize;
} elf_s_header;

#define SHN_UNDEF 	(0x00) //undefined/not present
enum elf_sh_types {
	SHT_NULL	= 0, //null section
	SHT_PROGBITS	= 1, //program information
	SHT_SYMTAB	= 2, //symbol table
	SHT_STRTAB	= 3, //string table
	SHT_RELA	= 4, //relocation (w/ addend)
	SHT_NOBITS	= 8, //not present in file
	SHT_REL 	= 9, //relocation (no addend)
};

enum elf_sh_attr {
	SHF_WRITE 	= 0x01, //writable section
	SHF_ALLOC	= 0x02, //exists in memory
};

enum elf_ident {
	EI_MAG0		= 0, //0x7F
	EI_MAG1		= 1, //'E'
	EI_MAG2		= 2, //'L'
	EI_MAG3		= 3, //'F'
	EI_CLASS	= 4, //arch (32/64)
	EI_DATA		= 5, //endianness
	EI_VERSION	= 6, //ELF version
	EI_OSABI 	= 7, //OS specific
	EI_ABIVERSION	= 8, //OS specific
	EI_PAD		= 9, //padding
};

#define ELFMAG0		0x7F //ident[EL_MAG0]
#define ELFMAG1		'E'  //ident[EL_MAG1]
#define ELFMAG2		'L'  //ident[EL_MAG2]
#define ELFMAG3		'F'  //ident[EL_MAG3]
#define ELFDATA2LSB	(1)  //little endian
#define ELFCLASS32	(1)  //32-bit arch

enum elf_type {
	ET_NONE		= 0, //unknown type
	ET_REL		= 1, //relocatable file
	ET_EXEC		= 2, //executable file
};

#define EM_386		(3)  //x86 type
#define EV_CURRENT	(1)  //ELF current version

#endif
