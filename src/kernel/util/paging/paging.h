#ifndef PAGING_H
#define PAGING_H

//All functions here are explicitly deprecated. The current implementations is in this include.
#include <kernel/vmm/vmm.h>

//retrieves pointer to page required
//if make == 1, if the page-table in which this page should
//reside isn't created, create it
page_t* get_page(uint32_t address, int make, page_directory_t* dir);
bool alloc_frame(page_t* page, int is_kernel, int is_writeable);
void free_frame(page_t* page);
//create a new page directory with all the info of src
//kernel pages are linked instead of copied
page_directory_t* clone_directory(page_directory_t* src);
//free all memory associated with a page directory dir
void free_directory(page_directory_t* dir);

void *mmap(void *addr, uint32_t length, int flags, int fd, uint32_t offset);
int munmap(void* addr, uint32_t length);
int brk(void* addr);
void* sbrk(int increment);

page_directory_t* page_dir_kern();
page_directory_t* page_dir_current();

#endif
