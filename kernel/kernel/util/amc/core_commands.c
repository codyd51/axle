#include <stdbool.h>
#include <stddef.h>
#include <std/array_m.h>
#include <std/kheap.h>
#include <std/printf.h>
#include <std/memory.h>
#include <std/string.h>
#include <std/math.h>

#include <kernel/boot_info.h>
#include <kernel/util/elf/elf.h>

#include "amc_internal.h"
#include "core_commands.h"
#include "kernel/drivers/pit/pit.h"
#include "kernel/pmm/pmm.h"

const uint64_t _AMC_SHARED_MEMORY_BASE = 0x7f0000000000;

static void _amc_core_copy_amc_services(const char* source_service) {
    printf("Request to copy services\n");
   
    array_m* services = amc_services();
    uint32_t response_size = sizeof(amc_service_list_t) + (sizeof(amc_service_description_t) * services->size);
    amc_service_list_t* service_list = kcalloc(1, response_size);
    service_list->event = AMC_COPY_SERVICES_RESPONSE;
    service_list->service_count = services->size;

    for (int i = 0; i < services->size; i++) {
        amc_service_description_t* service_desc = &service_list->service_descs[i];
        amc_service_t* service = array_m_lookup(services, i);
        //printf("Service desc 0x%08x, amc service 0x%08x %s -> desc 0x%08x 0x%08x\n", service_desc, service->name, service->name, service_desc->service_name, &service_desc->service_name);
        strncpy(&service_desc->service_name, service->name, AMC_MAX_SERVICE_NAME_LEN);
        NotImplemented();
        //service_desc->unread_message_count = service->message_queue->size;
    }
    amc_message_send__from_core(source_service, service_list, response_size);
    kfree(service_list);
}

static void _amc_core_awm_map_framebuffer(const char* source_service) {
    // Only awm is allowed to invoke this code!
    assert(!strncmp(source_service, "com.axle.awm", AMC_MAX_SERVICE_NAME_LEN), "Only AWM may use this syscall");

    amc_service_t* current_service = amc_service_with_name(source_service);
    //spinlock_acquire(&current_service->spinlock);
    framebuffer_info_t* framebuffer_info = &boot_info_get()->framebuffer;
    //  Map VESA framebuffer into proc's address space
    /*
    vmm_identity_map_region(
        (vmm_page_directory_t*)vmm_active_pdir(), 
        framebuffer_info->address,
        framebuffer_info->size
    );
    */
    //spinlock_release(&current_service->spinlock);

    // And set the pages as accessible to user-mode
    uint32_t framebuf_start_addr = framebuffer_info->address;
    uint32_t framebuf_end_addr = framebuffer_info->address + framebuffer_info->size;
    // Pad to page size
    framebuf_end_addr = (framebuf_end_addr + (PAGE_SIZE - 1)) & PAGING_PAGE_MASK;
    printf("Framebuffer: 0x%08x - 0x%08x (%d pages)\n", framebuf_start_addr, framebuf_end_addr, ((framebuf_end_addr - framebuf_start_addr) / PAGE_SIZE));
    /*
    for (uint32_t addr = framebuf_start_addr; addr < framebuf_end_addr; addr += PAGE_SIZE) {
        vmm_set_page_usermode(vmm_active_pdir(), addr);
    }
    */
    uint64_t framebuf_size = framebuf_end_addr - framebuf_start_addr;
    uint64_t mapped_framebuffer = vas_map_range(vas_get_active_state(), 0x7d0000000000, framebuf_size, framebuf_start_addr, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_USER);
    printf("Mapped framebuffer to 0x%p\n", mapped_framebuffer);

    amc_framebuffer_info_t msg = {
        .event = AMC_AWM_MAP_FRAMEBUFFER_RESPONSE,
        .address = mapped_framebuffer,
        .bits_per_pixel = framebuffer_info->bits_per_pixel,
        .bytes_per_pixel = framebuffer_info->bytes_per_pixel,
        .width = framebuffer_info->width,
        .height = framebuffer_info->height,
        .pixels_per_scanline = framebuffer_info->pixels_per_scanline,
        .size = framebuf_size,
    };
    amc_message_send__from_core(source_service, &msg, sizeof(amc_framebuffer_info_t));
}

static void _amc_core_put_service_to_sleep(const char* source_service, uint32_t ms, bool awake_on_message) {
    amc_service_t* service = amc_service_with_name(source_service);

    uint32_t now = ms_since_boot();
    uint32_t wake = now + ms;
    service->task->blocked_info.wake_timestamp = wake;
    //char* extra_msg = (awake_on_message) ? "or message arrives" : "(time only)";
    //printf("Core blocking %s [%d %s] at %d until %d %s (%dms)\n", source_service, service->task->id, service->task->name, now, wake, extra_msg, ms);

    array_m_insert(amc_sleeping_procs(), service);
    uint32_t block_reason = (awake_on_message) ? (AMC_AWAIT_TIMESTAMP | AMC_AWAIT_MESSAGE) : AMC_AWAIT_TIMESTAMP;
    tasking_block_task(service->task, block_reason);
}

static void _amc_core_file_server_map_initrd(const char* source_service) {
    // Only file_server is allowed to invoke this code!
    assert(!strncmp(source_service, "com.axle.file_server", AMC_MAX_SERVICE_NAME_LEN), "Only File Server may use this syscall");

    amc_service_t* current_service = amc_service_with_name(source_service);
    spinlock_acquire(&current_service->spinlock);

    // Map the ramdisk into the proc's address space
    boot_info_t* bi = boot_info_get();
    uint32_t page_padded_size = (bi->initrd_size + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
    uintptr_t mapped_initrd = vas_map_range(vas_get_active_state(), 0x7d0000000000, page_padded_size, bi->initrd_start, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_USER);
    spinlock_release(&current_service->spinlock);

    // And mark the pages as accessible to usermode
    printf("Ramdisk: 0x%p - 0x%p (%d pages)\n", mapped_initrd, mapped_initrd + bi->initrd_size, bi->initrd_size / PAGE_SIZE);

    amc_initrd_info_t msg = {
        .event = AMC_FILE_MANAGER_MAP_INITRD_RESPONSE,
        .initrd_start = mapped_initrd,
        .initrd_end = mapped_initrd + bi->initrd_size,
        .initrd_size = bi->initrd_size,
    };
    amc_message_send__from_core(source_service, &msg, sizeof(amc_initrd_info_t));
}

static void AMC_EXEC_TRAMPOLINE_NAME(const char* program_name, void* buf, uint32_t buf_size) {
    char* argv[] = {program_name, NULL};
    elf_load_buffer(program_name, argv, buf, buf_size, true);
	panic("noreturn");
}

static void _amc_core_file_server_exec_buffer(const char* source_service, void* buf, uint32_t buf_size) {
    //assert(!strncmp(source_service, "com.axle.file_server", AMC_MAX_SERVICE_NAME_LEN), "Only File Server may use this syscall");
    // This syscall is heavily restricted
    bool is_allowed_service = (
        !strncmp(source_service, "com.axle.file_server", AMC_MAX_SERVICE_NAME_LEN) || 
        !strncmp(source_service, "com.axle.linker", AMC_MAX_SERVICE_NAME_LEN) ||
        !strncmp(source_service, "com.axle.ide", AMC_MAX_SERVICE_NAME_LEN)
    );
    assert(is_allowed_service, "Only certain services may use this syscall");

    amc_exec_buffer_cmd_t* cmd = (amc_exec_buffer_cmd_t*)buf;
    printf("exec buffer(program_name: %s, buffer_addr: 0x%p, buffer_size: %p)\n", cmd->program_name, cmd->buffer_addr, cmd->buffer_size);
	printf("[%d ms] exec_buffer (buffer size %d)\n", ms_since_boot(), cmd->buffer_size);

    // Copy the buffer to kernel space
    char* copy = kmalloc(cmd->buffer_size);
    memcpy(copy, cmd->buffer_addr, cmd->buffer_size);
    // Copy program name to kernel space
    // TODO(PT): Where should this be freed?
    char* name_copy = strdup(cmd->program_name);

    if (cmd->with_supervisor) {
        task_small_t* child = task_spawn__managed__with_args(
            name_copy,
            AMC_EXEC_TRAMPOLINE_NAME, 
            (uintptr_t)name_copy, 
            (uintptr_t)copy, 
            cmd->buffer_size
        );
    }
    else {
        task_spawn__with_args(
            name_copy,
            AMC_EXEC_TRAMPOLINE_NAME, 
            (uintptr_t)name_copy, 
            (uintptr_t)copy, 
            cmd->buffer_size
        );
    }
    printf("[%d] Continuing from task_spawn\n", getpid());
}

static void _amc_core_handle_profile_request(const char* source_service) {
    amc_system_profile_response_t resp = {0};
    resp.event = AMC_SYSTEM_PROFILE_RESPONSE;
    //resp.pmm_allocated = pmm_allocated_memory();
    NotImplemented();
    resp.kheap_allocated = kheap_allocated_memory();
    amc_message_send__from_core(source_service, &resp, sizeof(resp));
}

static void _amc_core_handle_notify_service_died(const char* source_service, void* buf, uint32_t buf_size) {
    amc_service_t* source = amc_service_with_name(source_service);
    assert(source != NULL, "Failed to find service that sent the message...");

    amc_notify_when_service_dies_cmd_t* cmd = (amc_notify_when_service_dies_cmd_t*)buf;
    amc_service_t* remote = amc_service_with_name(&cmd->remote_service);
    if (!remote) {
        printf("Dropping request to notify on %s's death because it doesn't exist\n", cmd->remote_service);
        return;
    }

    array_m_insert(remote->services_to_notify_upon_death, source);
}

static void _amc_core_flush_messages_from_service_to_service(const char* source_service, void* buf, uint32_t buf_size) {
    amc_service_t* source = amc_service_with_name(source_service);
    assert(source != NULL, "Failed to find service that sent the message...");

    amc_flush_messages_to_service_cmd_t* cmd = (amc_flush_messages_to_service_cmd_t*)buf;
    amc_service_t* remote = amc_service_with_name(&cmd->remote_service);
    if (remote) {
        printf("Flushing messages from %s to %s from %s's delivery queue\n", source_service, cmd->remote_service, cmd->remote_service);
        NotImplemented();
        /*
        // We're modifying some state of the destination service - hold a spinlock
        spinlock_acquire(&remote->spinlock);
        for (int32_t i = remote->message_queue->size - 1; i >= 0; i--) {
            amc_message_t* msg = array_m_lookup(remote->message_queue, i);
            if (!strncmp(msg->source, source_service, AMC_MAX_SERVICE_NAME_LEN)) {
                amc_message_free(msg);
                array_m_remove(remote->message_queue, i);
            }
        }
        spinlock_release(&remote->spinlock);
        */
    }

    printf("Flushing messages from %s to %s from the undelivered message pool\n", source_service, cmd->remote_service);
    array_m* unknown_dest_service_message_pool = amc_messages_to_unknown_services_pool();
    printf("Undelivered size: %d\n", unknown_dest_service_message_pool->size);
    spinlock_acquire(&unknown_dest_service_message_pool->lock);
    for (int32_t i = unknown_dest_service_message_pool->size - 1; i >= 0; i--) {
        amc_message_t* msg = array_m_lookup(unknown_dest_service_message_pool, i);
        //printf("*** Undelivered: %s %s, check for %s %s\n", msg->source, msg->dest, source_service, cmd->remote_service);
        if (!strncmp(msg->source, source_service, AMC_MAX_SERVICE_NAME_LEN)) {
            if (!strncmp(msg->dest, cmd->remote_service, AMC_MAX_SERVICE_NAME_LEN)) {
                amc_message_free(msg);
                array_m_remove(unknown_dest_service_message_pool, i);
            }
        }
    }
    spinlock_release(&unknown_dest_service_message_pool->lock);
}

static void _amc_shared_memory_create(const char* source_service, void* buf, uint32_t buf_size) {
    amc_service_t* source = amc_service_with_name(source_service);
    assert(source != NULL, "Failed to find service that sent the message...");

    amc_shared_memory_create_cmd_t* cmd = (amc_shared_memory_create_cmd_t*)buf;
    amc_service_t* remote = amc_service_with_name(&cmd->remote_service_name);
    if (!remote) {
        printf("[AMC] Failed to find the requested remote service...\n");
        return;
    }
    //assert(remote != NULL, "Failed to find the requested remote service...");

    printf("[AMC] Creating shared memory [%s <-> %s] of size 0x%p\n", source->name, remote->name, cmd->buffer_size);

    uint64_t local_vas_base = vas_alloc_range(
        source->task->vas_state, 
        _AMC_SHARED_MEMORY_BASE, 
        cmd->buffer_size, 
        VAS_RANGE_ACCESS_LEVEL_READ_WRITE, 
        VAS_RANGE_PRIVILEGE_LEVEL_USER
    );
    uint64_t remote_vas_base = vas_copy_phys_mapping(
        remote->task->vas_state,
        source->task->vas_state, 
        _AMC_SHARED_MEMORY_BASE,
        cmd->buffer_size,
        local_vas_base,
        VAS_RANGE_ACCESS_LEVEL_READ_WRITE,
        VAS_RANGE_PRIVILEGE_LEVEL_USER
    );
    printf("[AMC] local VAS 0x%p remote VAS 0x%p\n", local_vas_base, remote_vas_base);

    amc_shared_memory_create_response_t msg = {
        .event = AMC_SHARED_MEMORY_CREATE_RESPONSE,
        .local_buffer_start = local_vas_base,
        .remote_buffer_start = remote_vas_base
    };
    amc_message_send__from_core(source_service, &msg, sizeof(amc_shared_memory_create_response_t));
}

static void _amc_query_service(const char* source_service, void* buf, uint32_t buf_size) {
    amc_service_t* source = amc_service_with_name(source_service);
    assert(source != NULL, "Failed to find service that sent the message...");

    amc_query_service_request_t* req = (amc_query_service_request_t*)buf;
    amc_service_t* remote = amc_service_with_name((const char*)req->remote_service_name);

    amc_query_service_response_t resp = {0};
    resp.event = AMC_QUERY_SERVICE_RESPONSE;
    strncpy(&resp.remote_service_name, &req->remote_service_name, AMC_MAX_SERVICE_NAME_LEN);
    resp.service_exists = remote != NULL;
    amc_message_send__from_core(source_service, &resp, sizeof(amc_query_service_response_t));
}

static void _amc_core_map_physical_range(const char* source_service, void* buf, uint32_t buf_size) {
    amc_service_t* source = amc_service_with_name(source_service);
    assert(source != NULL, "Failed to find service that sent the message...");

    amc_map_physical_range_request_t* req = (amc_map_physical_range_request_t*)buf;

    printf("[AMC] %s mapping physical range [0x%p - 0x%p]\n", source->name, req->phys_base, req->phys_base + req->size);

    uintptr_t virt_base = vas_map_range(vas_get_active_state(), 0x7d0000000000, req->size, req->phys_base, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_USER);
    printf("\tMapped physical range to virt [0x%p - 0x%p]\n", virt_base, virt_base + req->size);

    amc_map_physical_range_response_t resp = {0};
    resp.event = AMC_MAP_PHYSICAL_RANGE_RESPONSE;
    resp.virt_base = virt_base;
    amc_message_send__from_core(source_service, &resp, sizeof(resp));
}

static void _amc_core_alloc_physical_range(const char* source_service, void* buf, uint32_t buf_size) {
    amc_service_t* source = amc_service_with_name(source_service);
    assert(source != NULL, "Failed to find service that sent the message...");

    amc_alloc_physical_range_request_t* req = (amc_alloc_physical_range_request_t*)buf;

    printf("[AMC] %s allocating memory mapping of size 0x%p\n", source->name, req->size);

    uintptr_t phys_base = pmm_alloc_continuous_range(buf_size);
    uintptr_t virt_base = vas_map_range_exact(vas_get_active_state(), 0x7d0000000000, buf_size, phys_base, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_USER);
    printf("\tAllocated Phys [0x%p - 0x%p], Virt [0x%p - 0x%p]\n", phys_base, phys_base + req->size, virt_base, virt_base + req->size);

    amc_alloc_physical_range_response_t resp = {0};
    resp.event = AMC_ALLOC_PHYSICAL_RANGE_RESPONSE;
    resp.phys_base = phys_base;
    resp.virt_base = virt_base;
    amc_message_send__from_core(source_service, &resp, sizeof(resp));
}

static void _amc_core_free_physical_range(const char* source_service, void* buf, uint32_t buf_size) {
    amc_service_t* source = amc_service_with_name(source_service);
    assert(source != NULL, "Failed to find service that sent the message...");

    amc_free_physical_range_request_t* req = (amc_free_physical_range_request_t*)buf;

    printf("[AMC] %s freeing memory mapping: 0x%p - 0x%p\n", source->name, req->vaddr, req->vaddr + req->size);
    vas_free_range(vas_get_active_state(), req->vaddr, req->size);

    amc_free_physical_range_response_t resp = {0};
    resp.event = AMC_FREE_PHYSICAL_RANGE_RESPONSE;
    amc_message_send__from_core(source_service, &resp, sizeof(resp));
}

#define MEMWALKER_REQUEST_PML1_ENTRY 666
typedef struct memwalker_request_pml1 {
    uint32_t event; // MEMWALKER_REQUEST_PML1_ENTRY
} memwalker_request_pml1_t;

typedef struct pt_mapping {
    uint64_t pt_virt_base;
	uint64_t pt_phys_base;
    uint64_t mapped_memory_virt_base;
    uint64_t uninteresting_page_phys;
} pt_mapping_t;

typedef struct memwalker_request_pml1_response {
    uint32_t event; // MEMWALKER_REQUEST_PML1_ENTRY
    uint64_t pt_virt_base;
    uint64_t pt_phys_base;
    uint64_t mapped_memory_virt_base;
    uint64_t uninteresting_page_phys;
} memwalker_request_pml1_response_t;

#define TASK_VIEWER_GET_TASK_INFO 777
typedef struct task_viewer_get_task_info {
	uint32_t event;	// TASK_VIEWER_GET_TASK_INFO
} task_viewer_get_task_info_t;

typedef struct task_info {
    char name[64];
    uint32_t pid;
    uint64_t rip;
    uint64_t vas_range_count;
    // Don't send more than 16 ranges
    vas_range_t vas_ranges[16];
    uint64_t user_mode_rip;
    bool has_amc_service;
    uint64_t pending_amc_messages;
} task_info_t;

typedef struct task_viewer_get_task_info_response {
	uint32_t event; // TASK_VIEWER_GET_TASK_INFO
    uint32_t task_info_count;
    task_info_t tasks[];
} task_viewer_get_task_info_response_t;

uint64_t dangerous_map_pml1_entry(vas_state_t* vas_state, pt_mapping_t* out);

static void _amc_core_grant_pml1_entry(const char* source_service) {
    amc_service_t* source = amc_service_with_name(source_service);
    pt_mapping_t mapping = {0};
    dangerous_map_pml1_entry(vas_get_active_state(), &mapping);
    printf("Got virt 0x%p\n", mapping.pt_virt_base);
    memwalker_request_pml1_response_t resp = {
        .event = MEMWALKER_REQUEST_PML1_ENTRY,
        .pt_virt_base = mapping.pt_virt_base,
        .pt_phys_base = mapping.pt_phys_base,
        .mapped_memory_virt_base = mapping.mapped_memory_virt_base,
        .uninteresting_page_phys = mapping.uninteresting_page_phys,
    };
    amc_message_send__from_core(source_service, &resp, sizeof(resp));
}

#include <kernel/multitasking/tasks/task_small_int.h>
#include <kernel/util/amc/amc_internal.h>

task_viewer_get_task_info_response_t* tasking_populate_tasks_info(void);

static void _amc_core_send_task_info(const char* source_service) {
    // Don't let the process list change while we compute this
    spinlock_t scheduler_inspect_lock = {.name = "[Scheduler Inspect Lock]"};
    //spinlock_acquire(&scheduler_inspect_lock);
    kernel_begin_critical();

    task_viewer_get_task_info_response_t* response = tasking_populate_tasks_info();
    uint32_t response_size = sizeof(task_viewer_get_task_info_response_t) + (response->task_info_count * sizeof(task_info_t));
    printf("Response size %d\n", response_size);

    //spinlock_release(&scheduler_inspect_lock);
    kernel_end_critical();
    amc_message_send__from_core(source_service, response, response_size);
    kfree(response);
}

static void _amc_core_send_task_info2(const char* source_service) {
    // Don't let the process list change while we compute this
    spinlock_t scheduler_inspect_lock = {.name = "[Scheduler Inspect Lock]"};
    spinlock_acquire(&scheduler_inspect_lock);

    task_small_t* task_head = _tasking_get_linked_list_head();

    // First, count up all the tasks so we know how much memory we'll need
    int task_count = 1;
    task_small_t* node = task_head;
    while (node->next != NULL) {
        node = node->next;
        task_count += 1;
    }

    uint64_t response_size = sizeof(task_viewer_get_task_info_response_t) + (sizeof(task_info_t) * task_count);
    task_viewer_get_task_info_response_t* response = calloc(1, response_size);
    response->event = TASK_VIEWER_GET_TASK_INFO;
    response->task_info_count = task_count;

    vas_state_t* current_vas = vas_get_active_state();

    node = task_head;
    for (int i = 0; i < task_count; i++) {
        //vas_load_state(node->vas_state);
        //const char* name = node->name ?: "Unnamed task";
        //strncpy(response->tasks[i].name, name, sizeof(response->tasks[i].name));
        strncpy(response->tasks[i].name, "Test", sizeof(response->tasks[i].name));
        response->tasks[i].pid = node->id;
        response->tasks[i].rip = node->machine_state->rip;

        // Don't try to compute things via machine_state for the active process, 
        // because machine_state is computed on context switches away
        if (node != tasking_get_current_task()) {
            /*
            uint64_t* rbp = node->machine_state->rbp;
            uint64_t user_mode_rip = 0;
            for (int j = 0; j < 16; j++) {
                if (rbp < VAS_KERNEL_HEAP_BASE && !vas_is_page_present(vas_get_active_state(), (uint64_t)rbp)) {
                    printf("0x%x is unmapped\n in 0x%x\n", rbp, vas_get_active_state());
                    break;
                }
                //printf("%s 0x%x\n", node->name, rbp);
                /*
                if (rbp < USER_MODE_STACK_BOTTOM) {
                    break;
                }
                *
                uint64_t rip = rbp[1];

                // Is the RIP in the canonical lower-half? If so, it's probably a user-mode return address
                if ((rip & (1LL << 63LL)) == 0) {
                    user_mode_rip = rip;
                    break;
                }

                rbp = (uint64_t*)rbp[0];
            }

            response->tasks[i].user_mode_rip = user_mode_rip;
             */
            response->tasks[i].user_mode_rip = 0;
        }

        /*
        int vas_ranges_to_copy = MIN(
            sizeof(response->tasks[i].vas_ranges) / sizeof(response->tasks[i].vas_ranges[0]), 
            node->vas_state->range_count
        );
        vas_ranges_to_copy = 0;
        response->tasks[i].vas_range_count = vas_ranges_to_copy;
         */
        response->tasks[i].vas_range_count = 0;
        /*
        for (int j = 0; j < vas_ranges_to_copy; j++) {
            memcpy(&response->tasks[i].vas_ranges[j], &node->vas_state->ranges[j], sizeof(vas_range_t));
        }
        */

        // Copy AMC service info
        amc_service_t* service = amc_service_of_task(node);
        //printf("service 0x%x\n", service);
        response->tasks[i].has_amc_service = service != NULL;
        if (service != NULL) {
            response->tasks[i].pending_amc_messages = 0;
            //NotImplemented();
            //response->tasks[i].pending_amc_messages = service->message_queue->size;
        }

        node = node->next;
    }

    //printf("Finished, will reload current VAS\n");
    vas_load_state(current_vas);

    spinlock_release(&scheduler_inspect_lock);
    amc_message_send__from_core(source_service, response, response_size);
    kfree(response);
}

void amc_core_handle_message(const char* source_service, void* buf, uint32_t buf_size) {
    //printf("Message to core from %s\n", source_service);
    uint32_t* u32buf = (uint32_t*)buf;
    if (u32buf[0] == AMC_COPY_SERVICES) {
        _amc_core_copy_amc_services(source_service);
    }
    else if (u32buf[0] == AMC_AWM_MAP_FRAMEBUFFER) {
        _amc_core_awm_map_framebuffer(source_service);
    }
    else if (u32buf[0] == AMC_SLEEP_UNTIL_TIMESTAMP) {
        _amc_core_put_service_to_sleep(source_service, u32buf[1], false);
    }
    else if (u32buf[0] == AMC_SLEEP_UNTIL_TIMESTAMP_OR_MESSAGE) {
        _amc_core_put_service_to_sleep(source_service, u32buf[1], true);
    }
    else if (u32buf[0] == AMC_FILE_MANAGER_MAP_INITRD) {
        _amc_core_file_server_map_initrd(source_service);
    }
    else if (u32buf[0] == AMC_FILE_MANAGER_EXEC_BUFFER) {
        _amc_core_file_server_exec_buffer(source_service, buf, buf_size);
    }
    else if (u32buf[0] == AMC_SHARED_MEMORY_DESTROY) {
        assert(false, "shmem destroy amccmd");
        //_amc_core_shared_memory_destroy(source_service, buf, buf_size);
    }
    else if (u32buf[0] == AMC_SYSTEM_PROFILE_REQUEST) {
        _amc_core_handle_profile_request(source_service);
    }
    else if (u32buf[0] == AMC_REGISTER_NOTIFICATION_SERVICE_DIED) {
        _amc_core_handle_notify_service_died(source_service, buf, buf_size);
    }
    else if (u32buf[0] == AMC_FLUSH_MESSAGES_TO_SERVICE) {
        _amc_core_flush_messages_from_service_to_service(source_service, buf, buf_size);
    }
    else if (u32buf[0] == AMC_SHARED_MEMORY_CREATE_REQUEST) {
        _amc_shared_memory_create(source_service, buf, buf_size);
    }
    else if (u32buf[0] == AMC_QUERY_SERVICE_REQUEST) {
        _amc_query_service(source_service, buf, buf_size);
    }
    else if (u32buf[0] == AMC_MAP_PHYSICAL_RANGE_REQUEST) {
        _amc_core_map_physical_range(source_service, buf, buf_size);
    }
    else if (u32buf[0] == AMC_ALLOC_PHYSICAL_RANGE_REQUEST) {
        _amc_core_alloc_physical_range(source_service, buf, buf_size);
    }
    else if (u32buf[0] == AMC_FREE_PHYSICAL_RANGE_REQUEST) {
        _amc_core_free_physical_range(source_service, buf, buf_size);
    }
    else if (u32buf[0] == MEMWALKER_REQUEST_PML1_ENTRY) {
        _amc_core_grant_pml1_entry(source_service);
    }
    else if (u32buf[0] == TASK_VIEWER_GET_TASK_INFO) {
        _amc_core_send_task_info(source_service);
    }
    else {
        printf("Unknown message: %d\n", u32buf[0]);
        assert(0, "Unknown message to core");
        return;
    }
}

void task_inform_supervisor__process_create__with_task(task_small_t* task, uint64_t pid) {
    if (!task->is_managed_by_parent) {
        return;
    }
    amc_supervised_process_event_t msg = {0};
    msg.event = AMC_SUPERVISED_PROCESS_EVENT;
    msg.payload.discriminant = SupervisorEventProcessCreate;
    // getpid() can't be used here because this event is sent in the context of the parent
    msg.payload.fields.process_create_fields.pid = pid;
    amc_message_send__from_core(task->managing_parent_service_name, &msg, sizeof(msg));
}

void task_inform_supervisor__process_start(uint64_t entry_point) {
    task_small_t* current_task = tasking_get_current_task();
    if (!current_task->is_managed_by_parent) {
        return;
    }
    amc_supervised_process_event_t msg = {0};
    msg.event = AMC_SUPERVISED_PROCESS_EVENT;
    msg.payload.discriminant = SupervisorEventProcessStart;
    msg.payload.fields.process_start_fields.pid = getpid();
    msg.payload.fields.process_start_fields.entry_point = entry_point;
    amc_message_send__from_core(current_task->managing_parent_service_name, &msg, sizeof(msg));
}

void task_inform_supervisor__process_exit(uint64_t exit_code) {
    task_small_t* current_task = tasking_get_current_task();
    if (!current_task->is_managed_by_parent) {
        return;
    }
    amc_supervised_process_event_t msg = {0};
    msg.event = AMC_SUPERVISED_PROCESS_EVENT;
    msg.payload.discriminant = SupervisorEventProcessExit;
    msg.payload.fields.process_exit_fields.pid = getpid();
    msg.payload.fields.process_exit_fields.status_code = exit_code;
    amc_message_send__from_core(current_task->managing_parent_service_name, &msg, sizeof(msg));
}

void task_inform_supervisor__process_write(const char* buf, uint64_t len) {
    task_small_t* current_task = tasking_get_current_task();
    if (!current_task->is_managed_by_parent) {
        return;
    }
    amc_supervised_process_event_t msg = {0};
    msg.event = AMC_SUPERVISED_PROCESS_EVENT;
    msg.payload.discriminant = SupervisorEventProcessWrite;
    msg.payload.fields.process_write_fields.pid = getpid();
    msg.payload.fields.process_write_fields.len = len;
    strncpy((char*)msg.payload.fields.process_write_fields.msg, buf, len);
    amc_message_send__from_core(current_task->managing_parent_service_name, &msg, sizeof(msg));
}
