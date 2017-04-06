#include "exec.h"
#include <kernel/util/elf/elf.h>
#include <kernel/util/macho/macho.h>
#include <std/panic.h>
#include <kernel/util/paging/paging.h>
#include <kernel/util/multitasking/tasks/task.h>

typedef enum {
	ELF_TYPE,
	MACH_TYPE,
	UNKWN_TYPE,
} program_type;

static program_type find_program_type(FILE* file) {
	if (mach_validate(file)) {
		return MACH_TYPE;
	}
	else if (elf_validate(file)) {
		return ELF_TYPE;
	}
	return UNKWN_TYPE;
}

void wipe_page_tables(page_directory_t* page_dir) {
	page_regions_print(page_dir);
	page_directory_t* kern = page_dir_kern();
	for (int i = 0; i < 1024; i++) {
		page_table_t* tab = page_dir->tables[i];
		if (!tab) continue;

		//skip if this table is linked from kernel
		if (kern->tables[i] == tab) {
			//printf("skipping kern table %x\n", tab);
			continue;
		}

		printf("clearing non-kernel tab %x\n", tab);
		//clear all pages!
		for (int j = 0; j < 1024; j++) {
			if (!tab->pages[j].present) {
				continue;
			}
			//printf("idx %d present\n", j);

			//printf("clearing frame %d %x\n", j, tab->pages[j].frame);
			printf("%x\n", tab->pages[j].frame);
			//free_frame(&(tab->pages[j]));
			//tab->pages[j].present = 0;
		}
		kfree(tab);
		page_dir->tables[i] = NULL;
		page_dir->tablesPhysical[i] = NULL;
	}
}

int execve(const char *filename, char *const argv[], char *const envp[]) {
	//clear page tables from fork()
	task_t* current = task_with_pid(getpid());
	//wipe_page_tables(current->page_dir);

	printk("Loading binary %s\n", filename);
	FILE* file = fopen(filename, "rb");
	if (!file) {
		printf_err("Couldn't find file %s", filename);
		sys__exit(1);
	}

	program_type type = find_program_type(file);
	//program_type type = ELF_TYPE;
	if (type == UNKWN_TYPE) {
		printf_err("%s was not executable", filename);
		sys__exit(1);
	}
	else if (type == ELF_TYPE) {
		elf_load_file(filename, file, argv);
	}
	else if (type == MACH_TYPE) {
		mach_load_file(filename);
	}

	//we should never reach this point
	ASSERT(0, "execve didn't exit");
	sys__exit(1);
}

