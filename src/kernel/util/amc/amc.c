#include <std/kheap.h>
#include <std/math.h>
#include <std/array_l.h>
#include <std/array_m.h>
#include <std/hash_map.h>
#include <kernel/util/spinlock/spinlock.h>

#include "amc.h"
#include "amc_internal.h"

/* 
 * 0x80000000: ELF code and data
 * 0xa0000000: AMC shared-memory base (framebuffers)
 * 0xb0000000: AMC message delivery pool 
 * 0xd0000000: ELF user-mode stack
 */

/*
 * 2 approaches:
 *  - Message delivery pool
 *     * amc_message_t* msg = amc_message_await();
 *     * Client does not need to know message size in advance
 *     * Need to deal with reaching end-of-pool / not being able to fit message
 *     * But we already need to deal with this in the current approach
 *     * Needs some way to "release" a message
 *     * Or, clients are only allowed to process 1 message at a time
 *     * Once they fetch another message, the last message is invalid
 *     * (Always deliver at the base)
 *     * Seems good
 *  - Client provides a delivery buffer
 *     * buf[128]; amc_message_await(&buf, 128)
 *     * How does the client find out about messages too big to deliver?
 *     * Feels risky as the client has to guess message size
 *     * Does not need a way to release a message
 */
static const uint32_t _amc_delivery_pool_base = 0xb0000000;
static const uint32_t _amc_delivery_pool_size = 1024 * 1024 * 32;

static array_m* _amc_services = 0;

static const uint32_t _amc_messages_to_unknown_services_pool_size = 512;
static array_m* _amc_messages_to_unknown_services_pool = 0;

static array_m* _asleep_procs = 0;

static hash_map_t* _amc_services_by_name = 0;
static hash_map_t* _amc_services_by_task = 0;

static void _amc_message_add_to_delivery_queue(amc_service_t* dest_service, amc_message_t* message);
static void _amc_core_shared_memory_destroy(amc_service_t* local_service, uint32_t shmem_descriptor);
void _amc_remove_service_from_sleep_list(amc_service_t* service);

array_m* amc_services(void) {
    return _amc_services;
}

array_m* amc_sleeping_procs(void) {
    return _asleep_procs;
}

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
        printf("amc_service_with_name called before AMC had any registered services\n");
        return NULL;
        //panic("amc_service_with_name called before AMC had any registered services");
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
        //printf("No service for task 0x%08x\n", task);
    }
    return NULL;
    //panic("Didn't find amc service matching provided data");
}

amc_service_t* amc_service_with_name(const char* name) {
    return _amc_service_matching_data(name, NULL);
    //printf("_amc_service_with_name\n");
    //return hash_map_get(_amc_services_by_name, name, strlen(name));
}

static amc_service_t* _amc_service_of_task(task_small_t* task) {
    //printf("_amc_service_of_task\n");
    return _amc_service_matching_data(NULL, task);
    //return hash_map_get(_amc_services_by_task, task, sizeof(task));
}

static void _amc_deliver_pending_messages_to_new_service(amc_service_t* new_service) {
    // Track the messages we'll deliver to avoid modifying the array while iterating
    int message_indexes_to_deliver[_amc_messages_to_unknown_services_pool_size];
    int matching_msg_count = 0;

    spinlock_acquire(&_amc_messages_to_unknown_services_pool->lock);

    for (int i = 0; i < _amc_messages_to_unknown_services_pool->size; i++) {
        amc_message_t* msg = array_m_lookup(_amc_messages_to_unknown_services_pool, i);
        printf("New service %s, outstanding [%s -> %s]\n", new_service->name, msg->source, msg->dest);
        if (!strncmp(msg->dest, new_service->name, AMC_MAX_SERVICE_NAME_LEN)) {
            printf("Found outstanding msg from %s to new %s\n", msg->source, msg->dest);
            message_indexes_to_deliver[matching_msg_count++] = i;
        }
    }

    printf("[%d] Delivering %d outstanding messages to new service %s\n", getpid(), matching_msg_count, new_service->name);

    // Invert the loop in case we're draining the queue to empty
    // Maintain FIFO delivery but be cognisant of changing indexes
    int idx_shift = 0;
    for (int i = 0; i < matching_msg_count; i++) {
        int idx = message_indexes_to_deliver[i];
        idx -= idx_shift;
        amc_message_t* msg = array_m_lookup(_amc_messages_to_unknown_services_pool, idx);
        // Possible race due to _amc_messages_to_unknown_services_pool being unlocked?
        assert(!strncmp(msg->dest, new_service->name, AMC_MAX_SERVICE_NAME_LEN), "Message wasn't intended for this service!");

        array_m_remove(_amc_messages_to_unknown_services_pool, idx);
        idx_shift += 1;
        _amc_message_add_to_delivery_queue(new_service, msg);
    }

    spinlock_release(&_amc_messages_to_unknown_services_pool->lock);
}

void amc_register_service(const char* name) {
    assert(strlen(name) < AMC_MAX_SERVICE_NAME_LEN, "AMC service name exceeded max size!");
    if (!_amc_services) {
        // This could later be moved into a kernel-level amc_init()
        _amc_services = array_m_create(256);
        _amc_messages_to_unknown_services_pool = array_m_create(_amc_messages_to_unknown_services_pool_size);
        //_amc_services_by_name = hash_map_create();
        //_amc_services_by_task = hash_map_create();
        _asleep_procs = array_m_create(64);
    }

    task_small_t* current_task = tasking_get_current_task();
    if (_amc_service_matching_data(NULL, current_task)) {
        // The current process already has a registered service name
        panic("A process can expose only one service name");
    }
    if (amc_service_with_name(name) != NULL) {
        printf("invalid amc_register_service() will kill %s. AMC service name already registered\n", name);
        char buf[AMC_MAX_SERVICE_NAME_LEN];
        snprintf(buf, sizeof(buf), "com.axle.invalid-service-name-%d-%s", ms_since_boot(), name);
        amc_register_service(buf);
        task_assert(false, "AMC service name already registered", NULL);
        return;
    }

    amc_service_t* service = calloc(1, sizeof(amc_service_t));

    printf("Registering service with name 0x%08x (%s)\n", name, name);
    char buf[256];
    snprintf((char*)&buf, sizeof(buf), "[AMC spinlock for %s]", name);
    service->spinlock.name = strdup((char*)&buf);

    spinlock_acquire(&service->spinlock);

    // Rewrite the name of the task to match the amc service name
    task_set_name(current_task, name);

    // The provided string is mapped into the address space of the running process,
    // but isn't mapped into kernel-space.
    // Copy the string so we can access it in kernel-space
    service->name = strdup(name);
    service->task = current_task;
    service->message_queue = array_m_create(2048);
    service->shmem_regions = array_m_create(32);
    service->services_to_notify_upon_death = array_m_create(32);

    // Create the message delivery pool in the task's address space
	service->delivery_pool = vmm_alloc_continuous_range(
        (vmm_page_directory_t*)vmm_active_pdir(), 
        _amc_delivery_pool_size, 
        true, 
        _amc_delivery_pool_base, 
        true
    );
    printf("AMC delivery pool for %s at 0x%08x\n", name, service->delivery_pool);

    //hash_map_put(_amc_services_by_name, service->name, strlen(service->name), service);
    //hash_map_put(_amc_services_by_task, service->task, sizeof(service->task), service);
    array_m_insert(_amc_services, service);

    spinlock_release(&service->spinlock);

    // Deliver any messages sent to this service before it loaded
    _amc_deliver_pending_messages_to_new_service(service);
}

vmm_page_table_t* vas_virt_table_for_page_addr(vmm_page_directory_t* vas_virt, uint32_t page_addr, bool alloc);
uint32_t vmm_page_table_idx_for_virt_addr(uint32_t addr);
uint32_t vmm_page_idx_within_table_for_virt_addr(uint32_t addr);

void amc_teardown_service_for_task(task_small_t* task) {
    amc_service_t* service = _amc_service_of_task(task);
    if (!service) {
        // No AMC service for the provided task
        printf("AMC teardown: [%d %s] had no AMC service\n", task->id, task->name);
        return;
    }
    printf("AMC teardown: [%d %s] has an AMC service: %s\n", task->id, task->name, service->name);

    spinlock_acquire(&_amc_services->lock);

    /*
    // Remove from lookup tables
    printf("\tRemove from lookup tables\n");
    hash_map_delete(_amc_services_by_name, service->name, strlen(service->name));
    hash_map_delete(_amc_services_by_task, service->task, sizeof(service->task));
    */

    // Remove from list of amc services
    //printf("\tRemove from services list\n");
    int32_t idx = array_m_index(_amc_services, service);
    array_m_remove(_amc_services, idx);

    // Free message queue
    while (service->message_queue->size) {
        amc_message_t* msg = array_m_lookup(service->message_queue, 0);
        printf("\tFree undelivered message (%s -> %s)\n", msg->source, msg->dest);
        array_m_remove(service->message_queue, 0);
        amc_message_free(msg);
    }
    array_m_destroy(service->message_queue);

    // Free shared memory regions
    printf("Shared memory region size: %d\n", service->shmem_regions->size);
    while (service->shmem_regions->size) {
        printf("Free shared memory region\n");
        _amc_core_shared_memory_destroy(service, 0);
    }
    array_m_destroy(service->shmem_regions);

    // Inform other services that this service is now dead
    for (int32_t i = 0; i < service->services_to_notify_upon_death->size; i++) {
        amc_service_t* listener = array_m_lookup(service->services_to_notify_upon_death, i);
        printf("Informing %s of the death of %s\n", listener->name, service->name);
        amc_service_died_notification_t notif = {0};
        notif.event = AMC_SERVICE_DIED_NOTIFICATION;
        snprintf(&notif.dead_service, sizeof(notif.dead_service), "%s", service->name);
        amc_message_construct_and_send__from_core(listener->name, &notif, sizeof(notif));
    }
    array_m_destroy(service->services_to_notify_upon_death);

    //printf("\tTeardown metadata\n");
    // Free service metadata
    kfree(service->spinlock.name);
    kfree(service->name);

    // The amc delivery pool will be cleaned up on the global page dir teardown

    // Finally free the service control block itself
    kfree(service);
    spinlock_release(&_amc_services->lock);
    //printf("\tAMC teardown done\n");
}

static void _amc_print_inbox(amc_service_t* inbox) {
    printf("--------AMC inbox of %s-------\n", inbox->name);
    for (int i = 0; i < inbox->message_queue->size; i++) {
        amc_message_t* message = array_m_lookup(inbox->message_queue, i);
        //printf("dest %s\n", message->hdr.dest);
        //assert(message->hdr.dest == inbox->name, "name and dest dont match");
        printf("\tMessage from %s to %s\n", message->source, message->dest);
    }
    printf("--------------------------------------\n");
}

void amc_message_free(amc_message_t* msg) {
    kfree(msg);
}

static void _amc_message_add_to_delivery_queue(amc_service_t* dest_service, amc_message_t* message) {
    // We're modifying some state of the destination service - hold a spinlock
    spinlock_acquire(&dest_service->spinlock);

    if (dest_service->message_queue->size >= dest_service->message_queue->max_size - 16) {
        printf("Would exceed max size!\n");
        _amc_print_inbox(dest_service);
        printf("Task: 0x%08x machine state: 0x%08x\n", dest_service->task, dest_service->task->machine_state);
        assert(0, "would exceed");
    }

    // And add the message to its inbox
    array_m_insert(dest_service->message_queue, message);

    // And unblock the task if it was waiting for a message
    if ((dest_service->task->blocked_info.status & AMC_AWAIT_MESSAGE) != 0) {
        // Remove the service from the sleep list if it was there
        if ((dest_service->task->blocked_info.status & AMC_AWAIT_TIMESTAMP) != 0) {
            //printf("*** Remove %s from sleep list due to message arrival\n", dest_service->name);
            _amc_remove_service_from_sleep_list(dest_service);
        }
        tasking_unblock_task_with_reason(dest_service->task, false, AMC_AWAIT_MESSAGE);
    }

    // Release our exclusive access
    spinlock_release(&dest_service->spinlock);
}

void _amc_remove_service_from_sleep_list(amc_service_t* service) {
    spinlock_acquire(&_asleep_procs->lock);

    int32_t idx = array_m_index(_asleep_procs, service);
    assert(idx >= 0, "Can't remove service from sleep list because it wasn't there");
    array_m_remove(_asleep_procs, idx);

    spinlock_release(&_asleep_procs->lock);
}

bool amc_is_active(void) {
    return tasking_is_active() && _amc_services && _amc_services->size;
}

void amc_wake_sleeping_services(void) {
    if (!amc_is_active() || !_asleep_procs->size) {
        return;
    }

    uint32_t now = ms_since_boot();

    spinlock_acquire(&_asleep_procs->lock);

    while (true) {
        bool did_modify_list = false;
        for (int32_t i = 0; i < _asleep_procs->size; i++) {
            amc_service_t* s = array_m_lookup(_asleep_procs, i);
            if ((s->task->blocked_info.status & AMC_AWAIT_TIMESTAMP) == 0) {
                printf("Proc was in sleep list but wasn't asleep [%d %s]: %d\n", s->task->id, s->task->name, s->task->blocked_info.status);
            }
            if (now >= s->task->blocked_info.wake_timestamp) {
                // Wake the process
                _amc_remove_service_from_sleep_list(s);
                //printf("Wake up %s at %d\n", s->name, ms_since_boot());
                tasking_unblock_task_with_reason(s->task, false, AMC_AWAIT_TIMESTAMP);

                // And signal that we need to re-iterate as the indexes have changed
                // TODO(PT): We can store the start-index as an optimisation
                did_modify_list = true;
                break;
            }
        }
        if (!did_modify_list) {
            break;
        }
    }

    spinlock_release(&_asleep_procs->lock);
}

static void _amc_core_shared_memory_destroy(amc_service_t* local_service, uint32_t shmem_descriptor) {
    // For now, only awm is allowed to invoke this code
    // Later, extensive validations should be performed to ensure the shared memory region
    // described is really one we've set up
    // Otherwise, an attacker may be able to unmap memory from a victim at locations they control
    //assert(!strncmp(source_service, "com.axle.awm", AMC_MAX_SERVICE_NAME_LEN), "Only awm may use this syscall");

    //amc_shared_memory_destroy_cmd_t* cmd = (amc_shared_memory_destroy_cmd_t*)buf;
    //uint32_t size = cmd->shmem_size;
    //printf("Shared memory destroy. Local: %s [0x%08x - 0x%08x] Remote: %s [0x%08x - 0x%08x]\n", source_service, cmd->shmem_local, cmd->shmem_local + size, cmd->remote_service, cmd->shmem_remote, cmd->shmem_remote + size);
    printf("Shared memory destroy. Local: %s\n", local_service->name);
    pmm_dump();

    // Find the shared memory region in the local and remote
    if (local_service->shmem_regions->size <= shmem_descriptor) {
        assert(false, "Failed to find shmem region with the provided descriptor");
        return;
    }
    amc_shared_memory_region_t* local_shmem = array_m_lookup(local_service->shmem_regions, shmem_descriptor);
    printf("\tFound local shmem [%d %s]: Remote %s RemDesc %d Start 0x%08x Size 0x%08x\n", local_service->task->id, local_service->name, local_shmem->remote, local_shmem->remote_descriptor, local_shmem->start, local_shmem->size);

    amc_service_t* remote_service = amc_service_with_name(local_shmem->remote);
    assert(remote_service, "Failed to find remote service");

    if (remote_service->shmem_regions->size <= local_shmem->remote_descriptor) {
        assert(false, "Failed to find remote shmem region with the provided descriptor");
        return;
    }
    amc_shared_memory_region_t* remote_shmem = array_m_lookup(remote_service->shmem_regions, local_shmem->remote_descriptor);
    printf("\tFound remote shmem [%d %s]: Remote %s RemDesc %d Start 0x%08x Size 0x%08x\n", remote_service->task->id, remote_service->name, remote_shmem->remote, remote_shmem->remote_descriptor, remote_shmem->start, remote_shmem->size);

    //amc_service_t* remote_service = _amc_service_with_name(cmd->remote_service);

    vmm_page_directory_t* local_pdir = vas_active_map_phys_range(local_service->task->vmm, sizeof(vmm_page_directory_t));

    for (uint32_t i = 0; i < local_shmem->size; i += PAGING_PAGE_SIZE) {
        uint32_t local_page = local_shmem->start + i;

        //uint32_t frame = vmm_get_phys_address_for_mapped_page(vmm_active_pdir(), local_page);
        //printf("\tFree shared page 0x%08x frame 0x%08x\n", local_page, frame);
        //pmm_free(frame);

        // Map in the page table for this page
        uint32_t phys_table = vas_virt_table_for_page_addr(local_pdir, local_page, false);
        vmm_page_table_t* table = vas_active_map_temp(phys_table, PAGING_FRAME_SIZE);

        uint32_t page_idx = vmm_page_idx_within_table_for_virt_addr(local_page);
        uint32_t frame_addr = table->pages[page_idx].frame_idx * PAGING_FRAME_SIZE;

        // Free the page!
        // TODO(PT): Maybe should free it in the second loop to ensure no page 
        // faults if the remote proc accesses it while we do this
        pmm_free(frame_addr);

        assert(table->pages[page_idx].present, "Expected present page");
        memset(&table->pages[page_idx], 0, sizeof(vmm_page_t));
        // Mark the page as freed in the state bitmap
        vmm_bitmap_unset_addr(local_pdir, local_page);

        vas_active_unmap_temp(sizeof(vmm_page_table_t));
    }
    vmm_unmap_range(vmm_active_pdir(), local_pdir, sizeof(vmm_page_directory_t));

    // Unmap the region in the local VMM
    /*
    void vmm_unmap_range(vmm_page_directory_t* vmm_dir, uint32_t virt_start, uint32_t size);
    printf("Unmap local\n");
    vmm_unmap_range(vmm_active_pdir(), local_shmem->start, local_shmem->size);
    */

    printf("Load remote\n");
    vmm_page_directory_t* remote_pdir = vas_active_map_phys_range(remote_service->task->vmm, sizeof(vmm_page_directory_t));

    printf("Unmap remote\n");
    //vmm_unmap_range(virt_page_dir, cmd->shmem_remote, cmd->shmem_size);

    for (uint32_t i = 0; i < remote_shmem->size; i += PAGING_PAGE_SIZE) {
        uint32_t page_addr = remote_shmem->start + i;

        // Map in the page table for this page
        uint32_t phys_table = vas_virt_table_for_page_addr(remote_pdir, page_addr, false);
        vmm_page_table_t* table = vas_active_map_temp(phys_table, PAGING_FRAME_SIZE);

        uint32_t page_idx = vmm_page_idx_within_table_for_virt_addr(page_addr);
        uint32_t frame_addr = table->pages[page_idx].frame_idx * PAGING_FRAME_SIZE;
        //printf("\tFree AMC delivery pool page [P 0x%08x] [V 0x%08x]\n", frame_addr, page_addr);
        //_vmm_unmap_page(vmm_active_pdir(), i);
        assert(table->pages[page_idx].present, "Expected present page");
        memset(&table->pages[page_idx], 0, sizeof(vmm_page_t));
        // Mark the page as freed in the state bitmap
        vmm_bitmap_unset_addr(remote_pdir, page_addr);

        vas_active_unmap_temp(sizeof(vmm_page_table_t));
    }

    vmm_unmap_range(vmm_active_pdir(), remote_pdir, sizeof(vmm_page_directory_t));

    //array_m_remove(remote_service->shmem_regions, local_shmem->remote_descriptor);
    remote_service->shmem_regions->array[local_shmem->remote_descriptor] = NULL;
    kfree(remote_shmem);
    array_m_remove(local_service->shmem_regions, shmem_descriptor);
    kfree(local_shmem);

    /*
    uint32_t table_with_flags = virt_page_dir->table_pointers[i];
    uint32_t table_phys = (table_with_flags) & PAGING_FRAME_MASK;
    if (!(kernel_page_table_flags & PAGE_PRESENT_FLAG) && (table_with_flags & PAGING_FRAME_MASK)) {
        // Load in the page table so we can free each of its frames
        vmm_page_table_t* table = vas_active_map_temp(table_phys, PAGING_FRAME_SIZE);
        for (uint32_t j = 0; j < 1024; j++) {
            if (table->pages[j].present) {
                uint32_t frame_addr = table->pages[j].frame_idx * PAGING_FRAME_SIZE;
                //printf("Free page %d within table %d (0x%08x)\n", j, i, frame_addr);
                pmm_free(frame_addr);
            }
        }
        vas_active_unmap_temp(sizeof(vmm_page_table_t));

    vmm_unmap_range(vmm_active_pdir(), virt_page_dir, sizeof(vmm_page_directory_t));
    printf("Done\n");
    */

    // TODO(PT): Unmap the range in remote and local
    //void vmm_unmap_range(vmm_page_directory_t* vmm_dir, uint32_t virt_start, uint32_t size) {
    //uint32_t local_buffer = vmm_alloc_continuous_range(local_pdir, buffer_size, true, 0xa0000000, true);
}

static bool _amc_message_construct_and_send_from_service_name(const char* source_service,
                                                              const char* destination_service,
                                                              void* buf,
                                                              uint32_t buf_size) {
    if (buf_size >= AMC_MAX_MESSAGE_SIZE) printf("Large message size: %d\n", buf_size);
    assert(buf_size < AMC_MAX_MESSAGE_SIZE, "Message exceeded max size");
    assert(destination_service != NULL, "NULL destination service provided");

    // If this is a message to com.axle.core, provide special handling
    if (!strncmp(destination_service, AXLE_CORE_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
        amc_core_handle_message(source_service, buf, buf_size);
        return true;
    }

    uint32_t total_msg_size = buf_size + sizeof(amc_message_t);
    uint8_t* queued_msg = calloc(1, total_msg_size);
    amc_message_t* header = (amc_message_t*)queued_msg;
    strncpy((char*)header->source, source_service, sizeof(header->source));
    strncpy((char*)header->dest, destination_service, sizeof(header->dest));
    header->len = buf_size;
    memcpy(header->body, buf, buf_size);

    // Find the destination service
    amc_service_t* dest_service = amc_service_with_name(destination_service);

    if (dest_service == NULL) {
        printf("Dest service %s is null, adding to queue (size = %d)\n", destination_service, _amc_messages_to_unknown_services_pool->size);
        // The destination doesn't exist - store the message in a pool of
        // messages to unknown services
        array_m_insert(_amc_messages_to_unknown_services_pool, queued_msg);
        return false;
    }

    _amc_message_add_to_delivery_queue(dest_service, (amc_message_t*)queued_msg);
    return true;
}

bool amc_message_construct_and_send(const char* destination_service, void* buf, uint32_t buf_size) {
    amc_service_t* current_service = _amc_service_of_task(tasking_get_current_task());
    assert(current_service != NULL, "Current task is not a registered amc service");
    return _amc_message_construct_and_send_from_service_name(current_service->name,
                                                             destination_service,
                                                             buf,
                                                             buf_size);
}

bool amc_message_construct_and_send__from_core(const char* destination_service, void* buf, uint32_t buf_size) {
    return _amc_message_construct_and_send_from_service_name("com.axle.core",
                                                             destination_service,
                                                             buf,
                                                             buf_size);
}

// Asynchronously send the message to any service awaiting a message from this service
void amc_message_broadcast(amc_message_t* msg) {
    NotImplemented();
}

static void _amc_message_deliver(amc_service_t* service, amc_message_t* message, amc_message_t** out) {
    if (service->message_queue->size > 0 && service->message_queue->size % 10 == 0) {
        printf("%d AMC: Info [%d %s] inbox: %d\n", ms_since_boot(), service->task->id, service->name, service->message_queue->size);
    }
    uint8_t* delivery_base = (uint8_t*)service->delivery_pool;
    uint32_t total_msg_size = message->len + sizeof(amc_message_t);
    memcpy(delivery_base, (uint8_t*)message, total_msg_size);
    amc_message_free(message);
    *out = delivery_base;
}

void amc_message_await_from_services(int source_service_count, const char** source_services, amc_message_t** out) {
    amc_service_t* service = _amc_service_of_task(tasking_get_current_task());
    while (true) {
        // Hold a spinlock while iterating the service's messages
        // TODO(PT): Can we replace this with a "weaker" spinlock that 
        // doesn't disable interrupts?
        // Need to priority boost for the case:
        // awm is awaiting a message and gets preempted during this loop
        // the mouse driver processes an interrupt and wants to deliver 
        //  a message to awm
        // deadlock, because the awm is still holding its own lock
        spinlock_acquire(&service->spinlock);
        // Read messages in FIFO, from the array head to the tail
        for (int i = 0; i < service->message_queue->size; i++) {
            amc_message_t* queued_msg = array_m_lookup(service->message_queue, i);
            // A NULL array of source services means "any service"
            if (!source_services) {
                // Copy the message into the receiver's storage, and free the internal storage
                array_m_remove(service->message_queue, i);
                _amc_message_deliver(service, queued_msg, out);
                spinlock_release(&service->spinlock);
                return;
            }
            else {
                for (int service_name_idx = 0; service_name_idx < source_service_count; service_name_idx++) {
                    const char* source_service = source_services[service_name_idx];
                    if (!strcmp(source_service, queued_msg->source)) {
                        // Found a message that we're currently blocked for
                        // Copy the message into the receiver's storage, and free the internal storage
                        array_m_remove(service->message_queue, i);
                        _amc_message_deliver(service, queued_msg, out);
                        spinlock_release(&service->spinlock);
                        return;
                    }
                }
            }
        }
        // No message from a desired service is available
        // Block until we receive another message (from any service)
        // And release our lock for now
        spinlock_release(&service->spinlock);
        tasking_block_task(service->task, AMC_AWAIT_MESSAGE);

        // We've unblocked, so we now have a new message to read
        // Run the above loop again
    }
    assert(0, "Should never be reached");
}

// Block until a message has been received from the source service
void amc_message_await(const char* source_service, amc_message_t** out) {
    const char* services[] = {source_service, NULL};
    amc_message_await_from_services(1, services, out);
}

// Await a message from any service
// Blocks until a message is received
void amc_message_await_any(amc_message_t** out) {
    amc_message_await_from_services(0, NULL, out);
}

bool amc_has_message_from(const char* source_service) {
    amc_service_t* service = _amc_service_of_task(tasking_get_current_task());
    spinlock_acquire(&service->spinlock);

    for (int i = 0; i < service->message_queue->size; i++) {
        amc_message_t* message = array_m_lookup(service->message_queue, i);
        if (!strcmp(source_service, message->source)) {
            spinlock_release(&service->spinlock);
            return true;
        }
    }
    spinlock_release(&service->spinlock);
    return false;
}

bool amc_service_has_message(amc_service_t* service) {
    return service->message_queue->size > 0;
}

bool amc_has_message(void) {
    amc_service_t* service = _amc_service_of_task(tasking_get_current_task());
    return amc_service_has_message(service);
}

void amc_shared_memory_create(const char* remote_service_name, uint32_t buffer_size, uint32_t* local_buffer_ptr, uint32_t* remote_buffer_ptr) {
    amc_service_t* local_service = _amc_service_of_task(tasking_get_current_task());
    amc_service_t* remote_service = amc_service_with_name(remote_service_name);

    assert(local_service, "Failed to find local service");
    assert(remote_service, "Failed to find remote service");

    if (!remote_service) {
        //printf("Dropping shared memory request because service doesn't exist: %s\n", remote_service);
        // TODO(PT): Need some way to communicate failure to the caller
        //return;
    }

    spinlock_acquire(&local_service->spinlock);
    spinlock_acquire(&remote_service->spinlock);

    // Pad buffer size to page size if necessary
    if (buffer_size & PAGE_FLAG_BITS_MASK) {
        buffer_size = (buffer_size + PAGE_SIZE) & PAGING_PAGE_MASK;
    }

    // Map a buffer into the local address space
    vmm_page_directory_t* local_pdir = vmm_active_pdir();
    uint32_t local_buffer = vmm_alloc_continuous_range(local_pdir, buffer_size, true, 0xa0000000, true);
    memset((uint8_t*)local_buffer, 0, buffer_size);

    printf("Made local mapping: 0x%08x - 0x%08x\n", local_buffer, local_buffer+buffer_size);
    amc_shared_memory_region_t* shmem_local = calloc(1, sizeof(amc_shared_memory_region_t));
    snprintf(shmem_local->remote, sizeof(shmem_local->remote), "%s", remote_service_name);
    shmem_local->size = buffer_size;
    shmem_local->start = local_buffer;
    array_m_insert(local_service->shmem_regions, shmem_local);

    // Map a region in the destination task that points to the same physical memory
    // Map the remote VAS state into the active VAS
    vmm_page_directory_t* virt_remote_pdir = (vmm_page_directory_t*)vas_active_map_temp(remote_service->task->vmm, sizeof(vmm_page_directory_t));

    uint32_t remote_min_address = 0xa0000000;
    uint32_t remote_start = vmm_find_start_of_free_region(virt_remote_pdir, buffer_size, remote_min_address);
    //uint32_t remote_start = remote_min_address;
    printf("found free remote addr 0x%08x\n", remote_start);

    for (uint32_t i = 0; i < buffer_size; i += PAGE_SIZE) {
        // Find the physical address the region was mapped
        uint32_t phys_page = vmm_get_phys_address_for_mapped_page(local_pdir, local_buffer + i);
        uint32_t remote_addr = remote_start + i;
        _vas_virt_set_page_table_entry(virt_remote_pdir, remote_addr, phys_page, true, true, true);
        // Set the page table to user-mode as well
        uint32_t* page_tables = _get_page_tables_head(virt_remote_pdir);
        uint32_t table_idx = vmm_page_table_idx_for_virt_addr(remote_addr);
        uint32_t phys_table = page_tables[table_idx];
        if (!(phys_table & PAGE_USER_MODE_FLAG)) {
            //printf("Enabling user-mode flag on page table [%d] (phys|flags 0x%08x -> 0x%08x)\n", table_idx, phys_table, phys_table | PAGE_USER_MODE_FLAG);
            page_tables[table_idx] = phys_table | PAGE_USER_MODE_FLAG;
            invlpg(&page_tables[table_idx]);
            invlpg(phys_table);
        }
    }

    vas_active_unmap_temp(sizeof(vmm_page_directory_t));

    amc_shared_memory_region_t* shmem_remote = calloc(1, sizeof(amc_shared_memory_region_t));
    snprintf(shmem_remote->remote, sizeof(shmem_remote->remote), "%s", local_service->name);
    shmem_remote->size = buffer_size;
    shmem_remote->start = remote_start;
    array_m_insert(remote_service->shmem_regions, shmem_remote);

    shmem_local->remote_descriptor = remote_service->shmem_regions->size - 1;
    shmem_remote->remote_descriptor = local_service->shmem_regions->size - 1;

    //tasking_unblock_task(dest->task, false);

    printf("remote buffer: 0x%08x\n", remote_start);

    // Write the "out" info describing where memory was mapped
    *local_buffer_ptr = local_buffer;
    *remote_buffer_ptr = remote_start;

    spinlock_release(&remote_service->spinlock);
    spinlock_release(&local_service->spinlock);
}

#include <kernel/util/vfs/vfs.h>
static void _amc_launch_realtek_8139() {
    const char* program_name = "realtek_8139_driver";
    char* argv[] = {program_name, NULL};

    initrd_fs_node_t* node = vfs_find_initrd_node_by_name(program_name);
    uint32_t address = node->initrd_offset;
	elf_load_buffer(program_name, address, node->size, argv);
	panic("noreturn");
}

bool amc_launch_service(const char* service_name) {
    // TODO(PT): Eventually, this should iterate filesystem listings to 
    // find all the available AMC services, and launch the provided one
    // For now, hard-code known AMC services launched via this interface 
    // to process names
    if (!strcmp(service_name, "com.axle.realtek_8139_driver")) {
        task_spawn(_amc_launch_realtek_8139, PRIORITY_DRIVER, "");
        return true;
    }
    // TODO(PT): In the future, we could just return false when the name doesn't match any known service
    // For now, to detect errors, raise an assertion
    assert(0, "Cannot launch service: unknown name");
    return false;
}

void amc_physical_memory_region_create(uint32_t region_size, uint32_t* virtual_region_start_out, uint32_t* physical_region_start_out) {
    amc_service_t* current_service = _amc_service_of_task(tasking_get_current_task());
    spinlock_acquire(&current_service->spinlock);

    // Pad region size to page size
    if (region_size & PAGE_FLAG_BITS_MASK) {
        region_size = (region_size + PAGE_SIZE) & PAGING_PAGE_MASK;
    }

    uint32_t phys_start = pmm_alloc_continuous_range(region_size);
    vmm_page_directory_t* local_pdir = vmm_active_pdir();
    // PT: Force this region to be placed at a higher address to prevent conflicts with sbrk
    uint32_t virtual_region_start = vmm_map_phys_range__min_placement_addr(local_pdir, phys_start, region_size, 0xa0000000, true);

    printf("Made virtual mapping: 0x%08x - 0x%08x\n", virtual_region_start, virtual_region_start + region_size);
    printf("     Physicl mapping: 0x%08x - 0x%08x\n", phys_start, phys_start + region_size);

    // Write the "out" info describing where memory was mapped
    *virtual_region_start_out = virtual_region_start;
    *physical_region_start_out = phys_start;

    spinlock_release(&current_service->spinlock);
}

bool amc_service_is_active(const char* service) {
    if (!_amc_services) return false;
    return amc_service_with_name(service) != NULL;
}

amc_service_t* amc_service_of_active_task(void) {
    return _amc_service_of_task(tasking_get_current_task());
}

array_m* amc_messages_to_unknown_services_pool() {
    return _amc_messages_to_unknown_services_pool;
}
