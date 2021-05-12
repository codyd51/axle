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

#include <kernel/amc.h>

#include "syscalls.h"

//DEFN_SYSCALL(kill, 0);
//DEFN_SYSCALL(execve, 1, char*, char**, char**);
//DEFN_SYSCALL(open, 2, const char*, int);
//DEFN_SYSCALL(read, 3, int, char*, size_t);
//DEFN_SYSCALL(output, 4, int, char*);
DEFN_SYSCALL(yield, 5, int);
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
DEFN_SYSCALL(amc_message_await_any, 31, amc_message_t**);
DEFN_SYSCALL(amc_shared_memory_create, 32, const char*, uint32_t, uint32_t*, uint32_t*);
DEFN_SYSCALL(amc_has_message_from, 33, const char*);
DEFN_SYSCALL(amc_has_message, 34);
DEFN_SYSCALL(amc_launch_service, 35, const char*);
DEFN_SYSCALL(amc_physical_memory_region_create, 36, uint32_t, uint32_t*, uint32_t*);
DEFN_SYSCALL(amc_message_construct_and_send, 37, const char*, uint8_t*, uint32_t);

DEFN_SYSCALL(adi_register_driver, 38, const char*, uint32_t);
DEFN_SYSCALL(adi_event_await, 39, uint32_t);
DEFN_SYSCALL(adi_send_eoi, 40, uint32_t);

DEFN_SYSCALL(ms_since_boot, 41);

// According to the documentation, this is an acceptable minimal environ
// https://sourceware.org/newlib/libc.html#Syscalls
char* __env[1] = { 0 };
char** environ = __env;

/*
 * Implemented syscalls
 */

void yield(void) {
    // 1 maps to RUNNABLE
    sys_yield(1);
}

caddr_t sbrk(int incr) {
    return (caddr_t)sys_sbrk(incr);
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

// Asynchronously send the message to any service awaiting a message from this service
void amc_message_broadcast(amc_message_t* msg) {
    sys_amc_message_broadcast(msg);
}

// Block until a message has been received from the source service
void amc_message_await(const char* source_service, amc_message_t** out) {
    sys_amc_message_await(source_service, out);
}

// Block until a message has been received from any of the source services
void amc_message_await_from_services(int source_service_count, const char** source_services, amc_message_t** out) {
    sys_amc_message_await_from_services(source_service_count, source_services, out);
}

// Await a message from any service
// Blocks until a message is received
void amc_message_await_any(amc_message_t** out) {
    sys_amc_message_await_any(out);
}

void amc_shared_memory_create(const char* remote_service, uint32_t buffer_size, uint32_t* local_buffer, uint32_t* remote_buffer) {
    sys_amc_shared_memory_create(remote_service, buffer_size, local_buffer, remote_buffer);
}

void amc_physical_memory_region_create(uint32_t region_size, uint32_t* virtual_region_start_out, uint32_t* physical_region_start_out) {
    sys_amc_physical_memory_region_create(region_size, virtual_region_start_out, physical_region_start_out);
}

bool amc_has_message_from(const char* source_service) {
    return sys_amc_has_message_from(source_service);
}

bool amc_has_message(void) {
    return sys_amc_has_message();
}

bool amc_launch_service(const char* service_name) {
    return sys_amc_launch_service(service_name);
}

bool amc_message_construct_and_send(const char* destination_service,
                                    void* buf,
                                    uint32_t buf_size) {
    return sys_amc_message_construct_and_send(destination_service, buf, buf_size);
}

/*
 * ADI syscalls
 */

void adi_register_driver(const char* name, uint32_t irq) {
    sys_adi_register_driver(name, irq);
}

bool adi_event_await(uint32_t irq) {
    return sys_adi_event_await(irq);
}

void adi_send_eoi(uint32_t irq) {
    sys_adi_send_eoi(irq);
}

/*
 * Misc syscalls
 */

uint32_t ms_since_boot(void) {
    return sys_ms_since_boot();
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
