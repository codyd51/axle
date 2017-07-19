#ifndef SYSFUNCS_H
#define SYSFUNCS_H

#include "syscall.h"
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/util/vfs/fs.h>

//installs common syscalls into syscall table
void create_sysfuncs();

/*
//Standard terminal driver puts
DECL_SYSCALL2(output, int, char*);

//Standard terminal driver putc
DECL_SYSCALL1(terminal_putchar, char);

//Yeilds current process's running state to a different process
//Typically invoked if process is blocked by I/O, or sleeping
DECL_SYSCALL1(yield, task_state);

//Standard read syscall
//reads at most count characters into buf using file descriptor fd
DECL_SYSCALL3(read, int, void*, size_t);
*/

DECL_SYSCALL0(kill);
DECL_SYSCALL3(exec,		char*		, char**, char**);
DECL_SYSCALL2(open,		const char*	, int);
DECL_SYSCALL3(read,		int			, char*	, size_t);
DECL_SYSCALL2(output,	int			, char*);
DECL_SYSCALL1(yield,	task_state);
DECL_SYSCALL5(mmap,		void*		, int,	  int,		int,	int);
DECL_SYSCALL2(munmap,	void*		, int);
DECL_SYSCALL1(sbrk,		int);
DECL_SYSCALL3(lseek,	int			, int,	  int);
DECL_SYSCALL3(write,	int			, char*,  int);
DECL_SYSCALL0(fork);
DECL_SYSCALL0(getpid);
DECL_SYSCALL3(waitpid,	int			, int*,	  int);
DECL_SYSCALL1(task_with_pid,	int);

DECL_SYSCALL2(xserv_win_create,  Window*, Rect*);
DECL_SYSCALL1(xserv_win_present, Window*);
DECL_SYSCALL1(xserv_win_destroy, Window*);
DECL_SYSCALL0(xserv_init);

DECL_SYSCALL3(getdents, unsigned int, struct dirent*, unsigned int);

#endif
