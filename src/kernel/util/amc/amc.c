#include "amc.h"
#include <std/kheap.h>
#include <std/math.h>
#include <std/array_l.h>
#include <std/array_m.h>
#include <kernel/util/spinlock/spinlock.h>
#include <kernel/multitasking/tasks/task_small.h>

static array_m* _amc_services = 0;

typedef struct amc_service {
    const char* name;
    task_small_t* task;
	// Inbox of IPC messages awaiting processing
	//array_l* message_queue;
	array_m* message_queue;
    
    // Spinlock around interacting with a service
    spinlock_t spinlock;
} amc_service_t;

void amc_print_services(void) {
    if (!_amc_services) return;
    for (int i = 0; i < _amc_services->size; i++) {
        amc_service_t* service = array_m_lookup(_amc_services, i);
        //printf("AMC service: %s [%d %s]\n", service->name, service->task->id, service->task->name);
        printf("AMC service: %s [%d 0x%08x]\n", service->name, service->task->id, service->task);
    }
    printf("---\n");
}

// TODO(PT): Generic _amc_service_with_info(optional_task_struct, optional_service_name)... maybe returns a list?
static amc_service_t* _amc_service_matching_data(const char* name, task_small_t* task) {
    /*
     * Returns the first amc service matching the provided data.
     * Provide an attribute of the service to be found, or NULL.
     */
    // Ensure at least 1 attribute is provided
    if (!name && !task) {
        panic("Must provide at least 1 attribute to match");
        return NULL;
    }

    if (!_amc_services) {
        printf("_amc_service_with_name called before AMC had any registered services\n");
        return NULL;
        //panic("_amc_service_with_name called before AMC had any registered services");
    }

    // TODO(PT): We should be able to lock an array while iterating it
    for (int i = 0; i < _amc_services->size; i++) {
        //amc_service_t* service = array_l_lookup(_amc_services, i);
        amc_service_t* service = array_m_lookup(_amc_services, i);
        if (name != NULL && !strcmp(service->name, name)) {
            return service;
        }
        if (task != NULL && service->task == task) {
            return service;
        }
    }
    if (name) {
        //printf("No service with name %s\n", name);
    }
    else if (task) {
        printf("No service for task 0x%08x\n", task);
    }
    return NULL;
    //panic("Didn't find amc service matching provided data");
}

amc_service_t* _amc_service_with_name(const char* name) {
    return _amc_service_matching_data(name, NULL);
}

static amc_service_t* _amc_service_of_task(task_small_t* task) {
    return _amc_service_matching_data(NULL, task);
}

void amc_register_service(const char* name) {
    if (!_amc_services) {
        // This could later be moved into a kernel-level amc_init()
        //_amc_services = array_l_create();
        _amc_services = array_m_create(256);
    }

    task_small_t* current_task = tasking_get_current_task();
    /*
    if (current_task->amc_service_name != NULL) {
        // The current process already has a registered service name
        panic("A process can expose only one service name");
    }
    */

    amc_service_t* service = kmalloc(sizeof(amc_service_t));
    memset(service, 0, sizeof(amc_service_t));

    printf("Service 0x%08x\n", service);

    printf("Registering service with name 0x%08x (%s)\n", name, name);
    char buf[256];
    snprintf((char*)&buf, sizeof(buf), "[AMC spinlock for %s]", name);
    service->spinlock.name = strdup((char*)&buf);

    spinlock_acquire(&service->spinlock);
    // The provided string is mapped into the address space of the running process,
    // but isn't mapped into kernel-space.
    // Copy the string so we can access it in kernel-space
    service->name = strdup(name);
    service->task = current_task;
    assert(!interrupts_enabled(), "ints enabled during spinlock");
    service->message_queue = array_m_create(2048);

    //array_l_insert(_amc_services, service);
    array_m_insert(_amc_services, service);

    spinlock_release(&service->spinlock);

    printf("After register_service\n");
}

static amc_message_t* _amc_message_construct_from_service_name(const char* source_service_name, const char* data, int len) {
    assert(len < (AMC_MESSAGE_PAYLOAD_SIZE - sizeof(uint8_t)), "invalid len");
    //printf("_amc_message_construct_from_service_name(%s): %d %d %s\n", source_service_name, len, strlen(data), data);
    assert(source_service_name != NULL, "Must provide a source service");

    amc_message_t* out = kmalloc(sizeof(amc_message_t));
    memset(out, 0, sizeof(amc_message_t));

    out->hdr.source = source_service_name;

    // Cap length to the size of the `data` field
    //len = min(sizeof(out->data), len);
    //len = min(sizeof(&out->data), len);

    //memset(out->body.charlist.data, 0, sizeof(&out->body.charlist.data));

    //strncpy(out->data, data, len);
    out->body.charlist.len = len;
    memcpy(out->body.charlist.data, data, len);

    return out;
}

static void _amc_print_inbox(amc_service_t* inbox) {
    printf("--------AMC inbox of %s-------\n", inbox->name);
    for (int i = 0; i < inbox->message_queue->size; i++) {
        amc_message_t* message = array_m_lookup(inbox->message_queue, i);
        //printf("dest %s\n", message->hdr.dest);
        //assert(message->hdr.dest == inbox->name, "name and dest dont match");
        printf("\tMessage from %s to %s\n", message->hdr.source, message->hdr.dest);
    }
    printf("--------------------------------------\n");
}

amc_message_t* amc_message_construct(const char* data, int len) {
    amc_service_t* current_service = _amc_service_of_task(tasking_get_current_task());
    assert(current_service != NULL, "Current task is not a registered amc service");
    return _amc_message_construct_from_service_name(current_service->name, data, len);
}

amc_message_t* amc_message_construct__from_core(const char* data, int len) {
    // Allows syscalls to send messaages reported as originating from "com.axle.core" 
    // instead of the process that initiated the syscall
    amc_message_t* retval = _amc_message_construct_from_service_name("com.axle.core", data, len);
    return retval;
}

static void _amc_message_free(amc_message_t* msg) {
    kfree(msg);
}

// Asynchronously send the message to the provided destination service
bool amc_message_send(const char* destination_service, amc_message_t* msg) {
    // If a destination wasn't specified, the message should be broadcast globally
    if (destination_service == NULL) {
        NotImplemented();
    }
    // Find the destination service
    amc_service_t* dest = _amc_service_with_name(destination_service);
    // Copy the name from the service to ensure the string is mapped in kernel memory
    // (and thus is available in all processes)
    msg->hdr.dest = dest->name;

    if (dest == NULL) {
        printf("Dropping message because service doesn't exist: %s\n", destination_service);
        return false;
    }

    // We're modifying some state of the destination service - hold a spinlock
    spinlock_acquire(&dest->spinlock);

    // And add the message to its inbox
    array_m_insert(dest->message_queue, msg);

    // Release our exclusive access
    spinlock_release(&dest->spinlock);

    // And unblock the task if it was waiting for a message
    if (dest->task->blocked_info.status == AMC_AWAIT_MESSAGE) {
        //printf("AMC Unblocking [%d] %s\n", dest->task->id, dest->task->name);
        tasking_unblock_task(dest->task, false);

        // Higher priority tasks that were waiting on a message should preempt a lower priority active task
        if (dest->task->priority > get_current_task_priority()) {
            printf("[AMC] Jump to higher-priority task [%d] %s from [%d]\n", dest->task->id, dest->task->name, getpid());
            tasking_goto_task(dest->task);
        }
    }
    return true;
}

// Asynchronously send the message to any service awaiting a message from this service
void amc_message_broadcast(amc_message_t* msg) {
    NotImplemented();
}

void amc_message_await_from_services(int source_service_count, const char** source_services, amc_message_t* out) {
    amc_service_t* service = _amc_service_of_task(tasking_get_current_task());
    while (true) {
        // Hold a spinlock while iterating the service's messages
        //spinlock_acquire(&service->spinlock);
        // Read messages in FIFO, from the array head to the tail
        for (int i = 0; i < service->message_queue->size; i++) {
            //amc_message_t* message = array_l_lookup(service->message_queue, i);
            amc_message_t* message = array_m_lookup(service->message_queue, i);
            for (int service_name_idx = 0; service_name_idx < source_service_count; service_name_idx++) {
                const char* source_service = source_services[service_name_idx];
                if (!strcmp(source_service, message->hdr.source)) {
                    // Found a message that we're currently blocked for
                    array_m_remove(service->message_queue, i);
                    // Copy the message into the receiver's storage, and free the internal storage
                    memcpy(out, message, sizeof(amc_message_t));
                    _amc_message_free(message);

                    //spinlock_release(&service->spinlock);
                    return;
                }
            }
        }
        // No message from a desired service is available
        // Block until we receive another message (from any service)
        // And release our lock for now
        //spinlock_release(&service->spinlock);
        tasking_block_task(service->task, AMC_AWAIT_MESSAGE);

        // We've unblocked, so we now have a new message to read
        // Run the above loop again
    }
    assert(0, "Should never be reached");
}

// Block until a message has been received from the source service
void amc_message_await(const char* source_service, amc_message_t* out) {
    const char* services[] = {source_service};
    amc_message_await_from_services(1, services, out);

    /*
    if (strcmp(message->source, source_service)) {
        printf("%s received an unexpected message. Expected message from %s, but read message from %s\n", service->name, source_service, message->source);
        panic("Received message from unexpected service");
    }
    */
}

// Await a message from any service
// Blocks until a message is received
void amc_message_await_any(amc_message_t* out) {
    amc_service_t* service = _amc_service_of_task(tasking_get_current_task());
    if (service->message_queue->size == 0) {
        // No message available
        // Block until we receive another message (from any service)
        tasking_block_task(service->task, AMC_AWAIT_MESSAGE);
        // We've unblocked, so a message must be available
    }
    spinlock_acquire(&service->spinlock);
    assert(service->message_queue->size > 0, "amc_message_await_any continued without any messages to read");

    // Read messages in FIFO, from the array head to the tail
    amc_message_t* message = array_m_lookup(service->message_queue, 0);
    array_m_remove(service->message_queue, 0);
    // Copy the message into the receiver's storage, and free the internal storage
    memcpy(out, message, sizeof(amc_message_t));
    _amc_message_free(message);

    spinlock_release(&service->spinlock);
}

bool amc_has_message_from(const char* source_service) {
    amc_service_t* service = _amc_service_of_task(tasking_get_current_task());
    spinlock_acquire(&service->spinlock);

    for (int i = 0; i < service->message_queue->size; i++) {
        amc_message_t* message = array_m_lookup(service->message_queue, i);
        if (!strcmp(source_service, message->hdr.source)) {
            spinlock_release(&service->spinlock);
            return true;
        }
    }
    spinlock_release(&service->spinlock);
    return false;
}

bool amc_has_message(void) {
    amc_service_t* service = _amc_service_of_task(tasking_get_current_task());
    return service->message_queue->size > 0;
}

void amc_shared_memory_create(const char* remote_service, uint32_t buffer_size, uint32_t* local_buffer_ptr, uint32_t* remote_buffer_ptr) {
    // TODO(PT): Revisit and check everything works, add spinlock?
    amc_service_t* dest = _amc_service_with_name(remote_service);
    if (dest == NULL) {
        printf("Dropping shared memory request because service doesn't exist: %s\n", remote_service);
        // TODO(PT): Need some way to communicate failure to the caller
        return;
    }

    spinlock_acquire(&dest->spinlock);

    // Pad buffer size to page size
    buffer_size = (buffer_size + PAGE_SIZE) & PAGING_PAGE_MASK;

    // Map a buffer into the local address space
    vmm_page_directory_t* local_pdir = vmm_active_pdir();
    uint32_t local_buffer = vmm_alloc_continuous_range(local_pdir, buffer_size, true, 0xa0000000);

    printf("Made local mapping: 0x%08x - 0x%08x\n", local_buffer, local_buffer+buffer_size);

    // Map a region in the destination task that points to the same physical memory
    // And don't allow the destination task to be scheduled while we're modifying its VAS
    //tasking_block_task(dest->task, VMM_MODIFY);

    // Map the remote VAS state into the active VAS
    vmm_page_directory_t* virt_remote_pdir = (vmm_page_directory_t*)vas_active_map_temp(dest->task->vmm, sizeof(vmm_page_directory_t));
    printf("remote mapped pdir 0x%08x\n", virt_remote_pdir);

    uint32_t remote_min_address = 0xa0000000;
    uint32_t remote_start = vmm_find_start_of_free_region(virt_remote_pdir, buffer_size, remote_min_address);
    //uint32_t remote_start = remote_min_address;
    printf("found free remote addr 0x%08x\n", remote_start);

    for (uint32_t i = 0; i < buffer_size; i += PAGE_SIZE) {
        // Find the physical address the region was mapped
        uint32_t phys_page = vmm_get_phys_address_for_mapped_page(local_pdir, local_buffer + i);
        uint32_t remote_addr = remote_start + i;
        //printf("local 0x%08x phys 0x%08x to 0x%08x\n", local_buffer + i, phys_page, remote_addr);
        _vas_virt_set_page_table_entry(virt_remote_pdir, remote_addr, phys_page, true, true, false);
    }

    vas_active_unmap_temp(sizeof(vmm_page_directory_t));

    //tasking_unblock_task(dest->task, false);

    printf("remote buffer: 0x%08x\n", remote_start);

    // Write the "out" info describing where memory was mapped
    *local_buffer_ptr = local_buffer;
    *remote_buffer_ptr = remote_start;

    spinlock_release(&dest->spinlock);
}
