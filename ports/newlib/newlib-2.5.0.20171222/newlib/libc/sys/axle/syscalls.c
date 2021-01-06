/* note these headers are all provided by newlib - you don't need to provide them */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "syscalls.h"

// TODO(PT): Can we copy this definition from the sysroot?
typedef struct amc_message {
    const char* source;
    const char* dest; // May be null if the message is globally broadcast
    char data[64];
    int len;
} amc_message_t;

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
DEFN_SYSCALL(amc_register_service, 25, const char*);
DEFN_SYSCALL(amc_message_construct, 26, const char*, int);
DEFN_SYSCALL(amc_message_send, 27, const char*, amc_message_t*);
DEFN_SYSCALL(amc_message_broadcast, 28, amc_message_t*);
DEFN_SYSCALL(amc_message_await, 29, const char*, amc_message_t*);
DEFN_SYSCALL(amc_message_await_from_services, 30, int, const char**, amc_message_t*);
DEFN_SYSCALL(amc_message_await_any, 31, amc_message_t*);
DEFN_SYSCALL(amc_shared_memory_create, 32, const char*, uint32_t, uint32_t*, uint32_t*);

DEFN_SYSCALL(adi_register_driver, 33, const char*, uint32_t);
DEFN_SYSCALL(adi_interrupt_await, 34, uint32_t);

// According to the documentation, this is an acceptable minimal environ
// https://sourceware.org/newlib/libc.html#Syscalls
char* __env[1] = { 0 };
char** environ = __env;

/*
 * Implemented syscalls
 */

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
    // If sys_write returns an incorrect number of bytes written, 
    // newlib will loop to try and write more bytes
    return sys_write(file, ptr, len);
}

void amc_register_service(const char* name) {
    sys_amc_register_service(name);
}

/*
AMC syscalls
*/

// Construct an amc message
amc_message_t* amc_message_construct(const char* data, int len) {
    return sys_amc_message_construct(data, len);
}

// Asynchronously send the message to the provided destination service
bool amc_message_send(const char* destination_service, amc_message_t* msg) {
    return sys_amc_message_send(destination_service, msg);
}

// Asynchronously send the message to any service awaiting a message from this service
void amc_message_broadcast(amc_message_t* msg) {
    sys_amc_message_broadcast(msg);
}

// Block until a message has been received from the source service
void amc_message_await(const char* source_service, amc_message_t* out) {
    sys_amc_message_await(source_service, out);
}

// Block until a message has been received from any of the source services
void amc_message_await_from_services(int source_service_count, const char** source_services, amc_message_t* out) {
    sys_amc_message_await_from_services(source_service_count, source_services, out);
}

// Await a message from any service
// Blocks until a message is received
void amc_message_await_any(amc_message_t* out) {
    sys_amc_message_await_any(out);
}

void amc_shared_memory_create(const char* remote_service, uint32_t buffer_size, uint32_t* local_buffer, uint32_t* remote_buffer) {
    sys_amc_shared_memory_create(remote_service, buffer_size, local_buffer, remote_buffer);
}

/*
ADI syscalls
*/

void adi_register_driver(const char* name, uint32_t irq) {
    sys_adi_register_driver(name, irq);
}

void adi_interrupt_await(uint32_t irq) {
    sys_adi_interrupt_await(irq);
}

/*
 * Unimplemented syscall stubs
 */

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
