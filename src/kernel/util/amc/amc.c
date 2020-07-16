#include "amc.h"
#include <std/kheap.h>
#include <std/math.h>
#include <std/array_l.h>
#include <kernel/multitasking/tasks/task_small.h>

static array_l* _amc_services = 0;

typedef struct amc_service {
    const char* name;
    task_small_t* task;
	// Inbox of IPC messages awaiting processing
	array_l* message_queue;
    // TODO(PT): add a spinlock to this structure
} amc_service_t;

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
        panic("_amc_service_with_name called before AMC had any registered services");
    }

    // TODO(PT): We should be able to lock an array while iterating it
    for (int i = 0; i < _amc_services->size; i++) {
        amc_service_t* service = array_l_lookup(_amc_services, i);
        if (name != NULL && !strcmp(service->name, name)) {
            return service;
        }
        if (task != NULL && service->task == task) {
            return service;
        }
    }
    printf("No service with name %s task 0x%08x\n", name, task);
    return NULL;
    //panic("Didn't find amc service matching provided data");
}

static amc_service_t* _amc_service_with_name(const char* name) {
    return _amc_service_matching_data(name, NULL);
}

static amc_service_t* _amc_service_of_task(task_small_t* task) {
    return _amc_service_matching_data(NULL, task);
}

void amc_register_service(const char* name) {
    if (!_amc_services) {
        // This could later be moved into a kernel-level amc_init()
        _amc_services = array_l_create();
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

    printf("Registering service with name 0x%08x (%s)\n", name, name);
    // The provided string is mapped into the address space of the running process,
    // but isn't mapped into kernel-space.
    // Copy the string so we can access it in kernel-space
    service->name = strdup(name);
    service->task = current_task;
    service->message_queue = array_l_create();

    array_l_insert(_amc_services, service);
}

static amc_message_t* _amc_message_construct_from_service_name(const char* source_service_name, amc_message_type_t type, const char* data, int len) {
    assert(source_service_name != NULL, "Must provide a source service");

    amc_message_t* out = kmalloc(sizeof(amc_message_t));
    memset(out, 0, sizeof(amc_message_t));

    out->source = source_service_name;
    out->type = type;
    strncpy(out->data, data, min(sizeof(out->data), len));
    out->len = len;

    return out;
}

amc_message_t* amc_message_construct__from_core(amc_message_type_t type, const char* data, int len) {
    /*
    This method solves a problem when dispatching messages from an interrupt handler.
    An interrupt handler could be jumped to from any running process. Even though the 
    interrupt handler exists in kernel "core" code, any active task may be running when
    the interrupt handler is invoked. 
    Messages dispatched from an interrupt handler should always be reported as 
    originating from the core, so interrupt handlers that dispatch messages use this
    function instead of the amc_message_send() call.
    */
   return _amc_message_construct_from_service_name("com.axle.core", type, data, len);
}

amc_message_t* amc_message_construct(amc_message_type_t type, const char* data, int len) {
    amc_service_t* current_service = _amc_service_of_task(tasking_get_current_task());
    assert(current_service != NULL, "Current task is not a registered amc service");
    return _amc_message_construct_from_service_name(current_service->name, type, data, len);
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
    msg->dest = destination_service;

    // Find the destination service
    amc_service_t* dest = _amc_service_with_name(destination_service);
    if (dest == NULL) {
        printf("Dropping message because service doesn't exist: %s\n", destination_service);
        return false;
    }

    // And add the message to its inbox
    array_l_insert(dest->message_queue, msg);
    // And unblock the task if it was waiting for a message
    if (dest->task->blocked_info.status == AMC_AWAIT_MESSAGE) {
        tasking_unblock_task(dest->task, false);
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
        // Read messages in FIFO, from the array head to the tail
        for (int i = 0; i < service->message_queue->size; i++) {
            amc_message_t* message = array_l_lookup(service->message_queue, i);
            for (int service_name_idx = 0; service_name_idx < source_service_count; service_name_idx++) {
                const char* source_service = source_services[service_name_idx];
                if (!strcmp(source_service, message->source)) {
                    // Found a message that we're currently blocked for
                    array_l_remove(service->message_queue, i);
                    // Copy the message into the receiver's storage, and free the internal storage
                    memcpy(out, message, sizeof(amc_message_t));
                    _amc_message_free(message);
                    return;
                }
            }
        }
        // No message from a desired service is available
        // Block until we receive another message (from any service)
        tasking_block_task(service->task, AMC_AWAIT_MESSAGE);
        // We've unblocked, so we now have a new message to read
    }
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

}
