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

// amc syscalls
DEFN_SYSCALL(amc_register_service, 0, const char*);
DEFN_SYSCALL(amc_message_send, 1, const char*, uint8_t*, uint32_t);
DEFN_SYSCALL(amc_message_await, 2, const char*, amc_message_t*);
DEFN_SYSCALL(amc_message_await_from_services, 3, int, const char**, amc_message_t*);
DEFN_SYSCALL(amc_message_await_any, 4, amc_message_t**);
DEFN_SYSCALL(amc_has_message_from, 5, const char*);
DEFN_SYSCALL(amc_has_message, 6);

// TODO(PT): Perhaps amc_has_message and amc_message_await_any can be special usages of 
// their extended variants wrapped by libamc

// adi syscalls
DEFN_SYSCALL(adi_register_driver, 7, const char*, uint32_t);
DEFN_SYSCALL(adi_event_await, 8, uint32_t);
DEFN_SYSCALL(adi_send_eoi, 9, uint32_t);

// Processs management syscalls
DEFN_SYSCALL(sbrk, 10, int);
DEFN_SYSCALL(write, 11, int, char*, int);
DEFN_SYSCALL(_exit, 12, int);
DEFN_SYSCALL(getpid, 13);
DEFN_SYSCALL(ms_since_boot, 14);
DEFN_SYSCALL(task_assert, 15, const char*);

// According to the documentation, this is an acceptable minimal environ
// https://sourceware.org/newlib/libc.html#Syscalls
char* __env[1] = { 0 };
char** environ = __env;

/*
amc syscalls
*/

void amc_register_service(const char* name) {
    sys_amc_register_service(name);
}

bool amc_message_send(
    const char* destination_service,
    void* buf,
    uint32_t buf_size) {
    return sys_amc_message_send(destination_service, buf, buf_size);
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

bool amc_has_message_from(const char* source_service) {
    return sys_amc_has_message_from(source_service);
}

bool amc_has_message(void) {
    return sys_amc_has_message();
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

caddr_t sbrk(int incr) {
    return (caddr_t)sys_sbrk(incr);
}

int write(int file, char *ptr, int len) {
    // If sys_write returns an incorrect number of bytes written, 
    // newlib will loop to try and write more bytes
    return sys_write(file, ptr, len);
}
 
void _exit(int code) {
    sys__exit(code);
}

int getpid() {
    return sys_getpid();
}

int ms_since_boot(void) {
    return sys_ms_since_boot();
}

void task_assert(bool cond, const char* msg) {
    if (cond) {
        return;
    }
    sys_task_assert(msg);
}

/*
 * Unimplemented syscall stubs
 */

int execve(char *name, char **argv, char **env) {
    return -1;
}

int fork() {
    return -1;
}

int kill(int pid, int sig) {
    return -1;
}

int link(char *old, char *new) {
    return -1;
}

int fstat(int file, struct stat *st) {
    st->st_mode = S_IFCHR;
    return 0;
}

int isatty(int file) {
    return 1;
}

int stat(const char *file, struct stat *st) {
    st->st_mode = S_IFCHR;
    return 0;
}

int unlink(char *name) {
    return -1;
}

clock_t times(struct tms *buf) {
    return -1;
}

int wait(int *status) {
    return -1;
}

int gettimeofday(struct timeval *__restrict p, void *__restrict z) {
    return 0;
}

/*
 * Implemented in libfiles
 */

int close(int file) {
    assert(false, "Link against libfiles for file operations");
    return -1;
}

int lseek(int file, int ptr, int dir) {
    assert(false, "Link against libfiles for file operations");
    return -1;
}

int open(const char *name, int flags, ...) {
    assert(false, "Link against libfiles for file operations");
    return -1;
}

int read(int file, char *ptr, int len) {
    assert(false, "Link against libfiles for file operations");
    return -1;
}
