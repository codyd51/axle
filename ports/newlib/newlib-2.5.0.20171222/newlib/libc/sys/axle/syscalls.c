/* note these headers are all provided by newlib - you don't need to provide them */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <stdio.h>

#include "syscalls.h"

//DEFN_SYSCALL(kill, 0);
//DEFN_SYSCALL(execve, 1, char*, char**, char**);
//DEFN_SYSCALL(open, 2, const char*, int);
//DEFN_SYSCALL(read, 3, int, char*, size_t);
//DEFN_SYSCALL(output, 4, int, char*);
//DEFN_SYSCALL(yield, 5, task_state);
DEFN_SYSCALL(sbrk, 6, int);
//DEFN_SYSCALL(brk, 7, void*);
//DEFN_SYSCALL(mmap, 8, void*, int, int, int, int);
//DEFN_SYSCALL(munmap, 9, void*, int);
//DEFN_SYSCALL(lseek, 10, int, int, int);
DEFN_SYSCALL(write, 11, int, char*, int);
DEFN_SYSCALL(_exit, 12, int);
//DEFN_SYSCALL(fork, 13);
DEFN_SYSCALL(getpid, 14);
//DEFN_SYSCALL(waitpid, 15, int, int*, int);
//DEFN_SYSCALL(task_with_pid, 16, int);

// According to the documentation, this is an acceptable minimal environ
// https://sourceware.org/newlib/libc.html#Syscalls
char* __env[1] = { 0 };
char** environ = __env;

caddr_t sbrk(int incr) {
    return sys_sbrk(incr);
}
 
void _exit(int code) {
    sys__exit(code);
}

int getpid() {
    return sys_getpid();
}

int write(int file, char *ptr, int len) {
    return sys_write(file, ptr, len);
}

int close(int file) {
    return -1;
}

int execve(char *name, char **argv, char **env) {
    return -1;
}

int fork() {
    return -1;
}

int fstat(int file, struct stat *st) {
    st->st_mode = S_IFCHR;
    return 0;
}

int isatty(int file) {
    return 1;
}

int kill(int pid, int sig) {
    return -1;
}

int link(char *old, char *new) {
    return -1;
}

int lseek(int file, int ptr, int dir) {
    return 0;
}

int open(const char *name, int flags, ...) {
    return -1;
}

int read(int file, char *ptr, int len) {
    return 0;
}

int stat(const char *file, struct stat *st) {
    st->st_mode = S_IFCHR;
    return 0;
}

clock_t times(struct tms *buf) {
    return -1;
}

int unlink(char *name) {
    return -1;
}

int wait(int *status) {
    return -1;
}

int gettimeofday(struct timeval *__restrict p, void *__restrict z) {
    return 0;
}
