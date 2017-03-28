#ifndef KERNEL_ELF_H
#define KERNEL_ELF_H

#include "multiboot.h"

#define ELF32_ST_BIND(i) ((i)>>4)
#define ELF32_ST_TYPE(i) ((i)&0xf)
#define ELF32_ST_INFO(b, t)	(((b)<<4)+((t)&0xf))

typedef struct {
  uint32_t name;
  uint32_t type;
  uint32_t flags;
  uint32_t addr;
  uint32_t offset;
  uint32_t size;
  uint32_t link;
  uint32_t info;
  uint32_t addralign;
  uint32_t entsize;
} __attribute__((packed)) elf_section_header_t;
  
typedef struct {
  uint32_t name;
  uint32_t value;
  uint32_t size;
  uint8_t  info;
  uint8_t  other;
  uint16_t shndx;
} __attribute__((packed)) elf_symbol_t;

typedef struct {
  elf_symbol_t *symtab;
  uint32_t      symtabsz;
  const char   *strtab;
  uint32_t      strtabsz;
} elf_t;

//takes a multiboot struct and returns an elf struct containing relavent info
void elf_from_multiboot(multiboot* mb, elf_t* elf);
//elf_t elf_from_multiboot(multiboot* mb);

//looks up symbol at addr
const char* elf_sym_lookup(elf_t* elf, uint32_t addr);

#endif
