#include "exec.h"
#include <kernel/util/elf/elf.h>
#include <kernel/util/macho/macho.h>
#include <std/panic.h>
#include <kernel/util/paging/paging.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/util/plistman/plistman.h>
#include <kernel/util/syscall/sysfuncs.h>

int sys__exit(int code);

typedef enum {
	ELF_TYPE,
	MACH_TYPE,
	UNKWN_TYPE,
} program_type;

program_type find_program_type(FILE* file) {
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
		page_dir->tablesPhysical[i] = 0;
	}
}

plist_t* find_plist(const char* UNUSED(filename)) {
	plist_t* plist = NULL;
	/*
	struct dirent* ent;

	int i = 0;
	while ((ent = readdir_fs(fs_root, i++))) {
		if (strstr(ent->name, filename) && strstr(ent->name, ".plist")) {
			//plist found!
			printf("found plist for %s: %s\n", filename, ent->name);
			FILE* info = fopen(ent->name, "r");
			plist = kmalloc(sizeof(plist_t));
			plist_parse(plist, info);
			fclose(info);
			break;
		}
	}
	*/
	return plist;
}

int execve(const char *filename, char *const argv[], char *const UNUSED(envp[])) {
	/*
	printf("current directory phys %x\ kernel dir phys %x\n", page_dir_current()->physicalAddr, page_dir_kern()->physicalAddr);
	switch_page_directory(page_dir_kern());
	while (1) {}
	*/
	task_t* current = task_current();
	current->permissions = 0;

	/*
	plist_t* program_plist = find_plist(filename);
	if (program_plist) {
		for (int i = 0; i < program_plist->key_count; i++) {
			printf("%s = %s\n", program_plist->keys[i], program_plist->vals[i]);
			if (!strcmp(program_plist->keys[i], "id")) {
				current->name = strdup(program_plist->vals[i]);
			}
			if (!strcmp(program_plist->keys[i], "proc_master")) {
				if (!strcmp(program_plist->vals[i], "allow")) {
					current->permissions |= PROC_MASTER_PERMISSION;
					printf_info("granting task \"%s\" proc_master-allow.", current->name); 
					printf_info("this incident will be reported");
				}
			}
		}
		kfree(program_plist);
	}
	*/
	//clear page tables from fork()
	//task_t* current = task_with_pid(getpid());
	//wipe_page_tables(current->page_dir);
	
	/*
	extern volatile page_directory_t* current_directory;
	printf("EXEC current_directory %x kernel_directory %x\n", current_directory, page_dir_kern());
	switch_page_directory(page_dir_kern());
	printf("after switching, current %x\n", current_directory);
	return;
	*/
	//sys_exec_stage_2(filename);

	printk("Loading binary %s\n", filename);
	FILE* file = fopen(filename, "rb");
	if (!file) {
		printf_err("Couldn't find file %s", filename);
		sys__exit(1);
	}

	//program_type type = find_program_type(file);

	elf_load_file((char*)filename, file, (char**)argv);

	//we should never reach this point
	ASSERT(0, "execve didn't exit");
	sys__exit(1);
}

