#ifndef AMC_INTERNAL_H
#define AMC_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include <std/array_m.h>
#include <kernel/multitasking/tasks/task_small.h>

#include "amc.h"

// https://stackoverflow.com/questions/195975/how-to-make-a-char-string-from-a-c-macros-value
#define _STR_VALUE(arg)      #arg
#define _FUNCTION_NAME(name) _STR_VALUE(name)

// NOTE: This function name is special-cased in the kernel crash handler. 
#define AMC_EXEC_TRAMPOLINE_NAME _amc_exec_trampoline
#define AMC_EXEC_TRAMPOLINE_NAME_STR _FUNCTION_NAME(AMC_EXEC_TRAMPOLINE_NAME)

typedef struct amc_shared_memory_region {
    char remote[AMC_MAX_SERVICE_NAME_LEN];
    uintptr_t remote_descriptor;
    uintptr_t start;
    uint32_t size;
} amc_shared_memory_region_t;

typedef struct amc_service {
    char* name;
    task_small_t* task;
	// Inbox of IPC messages awaiting processing
	//array_l* message_queue;
	array_m* message_queue;
    
    // Spinlock around interacting with a service
    spinlock_t spinlock;

    // Base address of delivery pool
    uintptr_t delivery_pool;

    // Any shared memory regions that have been set up with another service
    array_m* shmem_regions;

    // Other amc services that have requested to receive a message when this service dies
    array_m* services_to_notify_upon_death;
} amc_service_t;

array_m* amc_services(void);

// Allows syscalls to send messages reported as originating from "com.axle.core" 
// instead of the process that initiated the syscall
bool amc_message_send__from_core(const char* destination_service, void* buf, uint32_t buf_size);

bool amc_service_has_message(amc_service_t* service);

void amc_wake_sleeping_services(void);

typedef struct task_small task_small_t;
typedef struct vmm_page_directory vmm_page_directory_t;
void amc_teardown_service_for_task(task_small_t* task);

array_m* amc_services(void);
array_m* amc_sleeping_procs(void);

amc_service_t* amc_service_with_name(const char* name);
amc_service_t* amc_service_of_task(task_small_t* task);

bool amc_is_active(void);

amc_service_t* amc_service_of_active_task(void);

void amc_message_free(amc_message_t* msg);
array_m* amc_messages_to_unknown_services_pool();

#endif
