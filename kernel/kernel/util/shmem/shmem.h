#ifndef SHMEM_H
#define SHMEM_H

#include <stdint.h>
#include <kernel/util/paging/paging.h>

char* shmem_create_map(page_directory_t* dir, uint8_t* address_to_map, uint32_t size, uint32_t begin_searching_at, bool writeable);
char* shmem_get_region_and_map(page_directory_t* dir, uint32_t size, uint32_t begin_searching_at, char** kernel_address, bool writeable);

char* find_unmapped_region(page_directory_t* dir, uint32_t size, uint32_t begin_searching_at);

#endif
