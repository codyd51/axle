#include "reaper.h"
#include "task_small.h"
#include "task_small_int.h"

#include <kernel/util/spinlock/spinlock.h>
#include <kernel/util/amc/amc.h>

void reaper_task(void) {
    amc_register_service("com.axle.reaper");
    spinlock_t reaper_lock = {0};
    reaper_lock.name = "[reaper lock]";

    while (1) {
        amc_message_t* msg;
        amc_message_await_any(&msg);
        if (strncmp(msg->source, AXLE_CORE_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
            printf("Reaper ignoring message from [%s]\n", msg->source);
            continue;
        }
        task_small_t** buf = (task_small_t**)msg->body;
        task_small_t* zombie_task = buf[0];

        spinlock_acquire(&reaper_lock);

        task_small_t* iter = _tasking_get_linked_list_head();
        task_small_t* prev = NULL;
        for (int i = 0; i < MAX_TASKS; i++) {
            if (iter == NULL) {
                assert(false, "Reaper failed to find provided task");
                return;
            }

            task_small_t* next = iter->next;
            if (iter == zombie_task) {
                assert(iter->blocked_info.status == ZOMBIE, "Status was not zombie");
                _thread_destroy(iter);

                // Remove this node from the linked-list of tasks
                if (prev != NULL) {
                    prev->next = next;
                }
                else {
                    _tasking_set_linked_list_head(next);
                }
                printf("Reaper freed corpse [%d %s]\n", zombie_task->id, zombie_task->name);
                break;
            }
            prev = iter;
            iter = (iter)->next;
        }

        spinlock_release(&reaper_lock);
    }
}
