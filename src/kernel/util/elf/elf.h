#ifndef ELF_H
#define ELF_H

#include <stdint.h>

#define ELF_RELOC_ERR -1

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

typedef struct {
	uint32_t	type;			/* Segment type */
	uint32_t	offset;		/* Segment file offset */
	uint32_t	vaddr;		/* Segment virtual address */
	uint32_t	paddr;		/* Segment physical address */
	uint32_t	filesz;		/* Segment size in file */
	uint32_t	memsz;		/* Segment size in memory */
	uint32_t	flags;		/* Segment flags */
	uint32_t	align;		/* Segment alignment */
} elf_phdr;

typedef struct {
	uint32_t 	name;
	uint32_t 	type;
	uint32_t 	flags;
	uint32_t 	addr;
	uint32_t 	offset;
	uint32_t 	size;
	uint32_t 	link;
	uint32_t 	info;
	uint32_t 	addralign;
	uint32_t 	entsize;
} elf_s_header;

typedef struct {
	uint32_t 	name;
	uint32_t 	value;
	uint32_t 	size;
	uint8_t 	info;
	uint8_t 	other;
	uint16_t 	shndx;
} elf_sym_tab;

#define ELF32_ST_BIND(INFO)	((INFO) >> 4)
#define ELF32_ST_TYPE(INFO) 	((INFO) & 0x0F)

enum elf_sym_tab_bindings {
	STB_LOCAL	= 0, //local scope
	STB_GLOBAL	= 1, //global scope
	STB_WEAK 	= 2, //weak (__attribute__((weak)))
};

enum elf_sym_tab_types {
	STT_NOTYPE	= 0, //no type
	STT_OBJECT	= 1, //variable/array/etc
	STT_FUNC	= 2, //method/function
};

#define SHN_UNDEF 	(0x00) //undefined/not present
#define SHN_ABS		(0xfff1)
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

typedef struct {
	uint32_t 	offset;
	uint32_t 	info;
} elf_rel;

typedef struct {
	uint32_t offset;
	uint32_t info;
	int32_t addend;
} elf_rela;

#define ELF_R_SYM(INFO)	((INFO) >> 8)
#define ELF_R_TYPE(INFO)((uint8_t)(INFO))

enum elf_rt_types {
	R_386_NONE 	= 0, //no relocation
	R_386_32	= 1, //symbol + offset
	R_386_PC32	= 2, //symbol + offset - section offset
};

#define PT_LOAD		1
#define PT_DYNAMIC	2
#define PT_INTERP	3


void* elf_load_file(char* filename, void* file, uint32_t size);

int execve(const char *filename, char *const argv[], char *const envp[]);

#endif
