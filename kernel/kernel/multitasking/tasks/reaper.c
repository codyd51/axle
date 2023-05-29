#include "reaper.h"
#include "task_small.h"
#include "task_small_int.h"
#include "mlfq.h"

#include <std/printf.h>
#include <std/string.h>

#include <kernel/util/spinlock/spinlock.h>
#include <kernel/util/amc/amc.h>
#include <kernel/util/amc/amc_internal.h>
#include <kernel/assert.h>
#include <kernel/kernel.h>

void reaper_task(void) {
    amc_register_service("com.axle.reaper");
    // Immediately yield so that the scheduler can continue with its startup without waiting for preemption
    task_switch();

    spinlock_t reaper_lock = {0};
    reaper_lock.name = "[reaper lock]";

    while (1) {
        amc_message_t* msg;
        amc_message_await_any(&msg);
        if (strncmp(msg->source, AXLE_CORE_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
            printf("Reaper ignoring message from [%s]\n", msg->source);
            continue;
        }
        // TODO(PT): Stop using the `next` field as it'll no longer be a well-formed linked list
        task_small_t** buf = (task_small_t**)msg->body;
        task_small_t* zombie_task = buf[0];

        spinlock_acquire(&reaper_lock);

        assert(zombie_task->blocked_info.status == ZOMBIE, "Status was not zombie");
        printf("Reaper freeing corpse [%d %s]\n", zombie_task->id, zombie_task->name);

        _thread_destroy(zombie_task);

        /*
        // We need to relaunch tasks here to avoid a race condition.
        // If we tried to relaunch a task before the call to _thread_destroy(),
        // the new task might be scheduled before the old task's amc service is cleaned up.
        amc_service_t* zombie_task_amc_service = amc_service_of_task(zombie_task);
        // TODO(PT): Have a lookaside table. If the file server crashes more than a few times, fatal OS error
        bool should_relaunch_task = zombie_task_amc_service && should_relaunch_crashed_amc_service(zombie_task_amc_service);
        */

        spinlock_release(&reaper_lock);
    }
}
