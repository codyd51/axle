#ifndef SYSFUNCS_H
#define SYSFUNCS_H

#include "syscall.h"
#include <kernel/multitasking/tasks/task.h>
#include <kernel/util/vfs/fs.h>
#include <kernel/util/amc/amc.h>

//installs common syscalls into syscall table
void create_sysfuncs();

/*
//Standard terminal driver puts
DECL_SYSCALL(output, int, char*);

//Standard terminal driver putc
DECL_SYSCALL(terminal_putchar, char);

//Yeilds current process's running state to a different process
//Typically invoked if process is blocked by I/O, or sleeping
DECL_SYSCALL(yield, task_state);

//Standard read syscall
//reads at most count characters into buf using file descriptor fd
DECL_SYSCALL(read, int, void*, size_t);
*/

DECL_SYSCALL(kill);
DECL_SYSCALL(exec, char*, char**, char**);
DECL_SYSCALL(open, const char*, int);
DECL_SYSCALL(read, int, char*, size_t);
DECL_SYSCALL(output, int, char*);
DECL_SYSCALL(yield, task_state);
DECL_SYSCALL(mmap, void*, int, int, int, int);
DECL_SYSCALL(munmap, void*, int);
DECL_SYSCALL(sbrk, int);
DECL_SYSCALL(lseek, int, int, int);
DECL_SYSCALL(write, int, char*, int);
DECL_SYSCALL(fork);
DECL_SYSCALL(getpid);
DECL_SYSCALL(waitpid, int, int*, int);
DECL_SYSCALL(task_with_pid, int);

DECL_SYSCALL(xserv_win_create, Window*, Rect*);
DECL_SYSCALL(xserv_win_present, Window*);
DECL_SYSCALL(xserv_win_destroy, Window*);
DECL_SYSCALL(xserv_init);

DECL_SYSCALL(getdents, unsigned int, struct dirent*, unsigned int);
DECL_SYSCALL(shmem_create, uint32_t);
DECL_SYSCALL(surface_create, uint32_t, uint32_t);
DECL_SYSCALL(aipc_send, char*, uint32_t, uint32_t, char**);

DECL_SYSCALL(amc_register_service, const char*);
DECL_SYSCALL(amc_message_construct, const char*, int);
DECL_SYSCALL(amc_message_send, const char*, amc_message_t*);
DECL_SYSCALL(amc_message_broadcast, amc_message_t*);
DECL_SYSCALL(amc_message_await, const char*, amc_message_t**);
DECL_SYSCALL(amc_message_await_from_services, int, const char**, amc_message_t**);
DECL_SYSCALL(amc_message_await_any, amc_message_t**);
DECL_SYSCALL(amc_shared_memory_create, const char*, uint32_t, uint32_t*, uint32_t*);

#endif
