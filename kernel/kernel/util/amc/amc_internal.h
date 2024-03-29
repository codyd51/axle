#ifndef AMC_INTERNAL_H
#define AMC_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include <std/array_m.h>
#include <kernel/multitasking/tasks/task_small.h>

#include "amc.h"

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
	array_m* message_queue;
    
    // Spinlock around interacting with a service
    spinlock_t spinlock;

    // Base address of delivery pool
    uintptr_t delivery_pool;

    // Any shared memory regions that have been set up with another service
    array_m* shmem_regions;

    // Other amc services that have requested to receive a message when this service dies
    array_m* services_to_notify_upon_death;

    // Whether the service is able to receive messages. This is unset when a service crashes.
    bool delivery_enabled;
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

void amc_disable_delivery(amc_service_t* service);

void task_inform_supervisor__process_create__with_task(task_small_t* task, uint64_t pid);
void task_inform_supervisor__process_start(uint64_t entry_point);
void task_inform_supervisor__process_exit(uint64_t exit_code);
void task_inform_supervisor__process_write(const char* buf, uint64_t len);

#endif
