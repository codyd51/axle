#include "adi.h"
#include <std/math.h>
#include <kernel/interrupts/pic.h>
#include <kernel/util/amc/amc_internal.h>
#include <kernel/util/spinlock/spinlock.h>
#include <kernel/multitasking/tasks/task_small.h>

#define MAX_IRQ 64
#define MAX_INT_VECTOR 128
static adi_driver_t _adi_drivers[MAX_IRQ] = {0};

static adi_driver_t* _adi_driver_matching_data(const char* name, task_small_t* task, uint32_t irq) {
    /*
     * Returns the first adi driver matching the provided data.
     * Provide an attribute of the service to be found, or NULL.
     */
    // Ensure at least 1 attribute is provided
    if (!name && !task && !irq) {
        panic("Must provide at least 1 attribute to match");
        return NULL;
    }

    for (uint32_t i = 0; i < sizeof(_adi_drivers) / sizeof(_adi_drivers[0]); i++) {
        adi_driver_t* driver = _adi_drivers + i;
        if (name != NULL && driver->name != NULL && !strcmp(driver->name, name)) {
            return driver;
        }
        if (task != NULL && driver->task != NULL && driver->task == task) {
            return driver;
        }
        if (irq != 0 && driver->irq != 0 && irq == driver->irq) {
            return driver;
        }
    }
    return NULL;
}

static void _adi_interrupt_handler(register_state_t* regs) {
    // Wake up the driver thread that services this interrupt
    int irq = regs->int_no;
    assert(irq > 0 && irq < MAX_INT_VECTOR, "Invalid IRQ provided");
    assert(_adi_drivers[irq].task != NULL, "IRQ does not have a corresponding driver");

    adi_driver_t* driver = _adi_drivers + irq;
    driver->pending_irq_count += 1;

    task_small_t* task = (task_small_t*)driver->task;
    tasking_unblock_task_with_reason(task, false, IRQ_WAIT);
    mlfq_goto_task(task);
}

void adi_register_driver(const char* name, uint32_t irq) {
    task_assert(irq > 0 && irq < MAX_INT_VECTOR, "Invalid IRQ provided", NULL);
    if (_adi_drivers[irq].task) {
        printf("invalid adi_register_driver() will kill %s. IRQ already mapped to a driver task\n", name);
        task_assert(false, "IRQ already mapped to a driver task", NULL);
        return;
    }

    task_small_t* current_task = tasking_get_current_task();

    // We're modifying interrupt handling - clear interrupts
    static spinlock_t int_spinlock = {0};
    if (!int_spinlock.name) int_spinlock.name = "adi_register_driver interrupt clear";
    spinlock_acquire(&int_spinlock);

    // Elevate the task's priority since it's a device driver
    spinlock_acquire(&current_task->priority_lock);
    current_task->priority = PRIORITY_DRIVER;
    spinlock_release(&current_task->priority_lock);

    _adi_drivers[irq].task = current_task;
    // The provided string is mapped into the address space of the running process,
    // but isn't mapped into kernel-space.
    // Copy the string so we can access it in kernel-space
    _adi_drivers[irq].name = strdup(name);
    _adi_drivers[irq].pending_irq_count = 0;
    printf("Mapped _adi_drivers[%d] = %s\n", irq, _adi_drivers[irq].name);

    // Set up an interrupt handler that will unblock the driver process
    interrupt_setup_callback(irq, _adi_interrupt_handler);

    spinlock_release(&int_spinlock);
}

bool adi_event_await(uint32_t irq) {
    spinlock_t s = {0};
    if (!s.name) s.name = "adi_event_wait spin";
    spinlock_acquire(&s);
    assert(irq > 0 && irq < MAX_INT_VECTOR, "Invalid IRQ provided");
    if (_adi_drivers[irq].task != tasking_get_current_task()) {
        printf("adi maps IRQ %d to %s, but current task is %s\n", irq, _adi_drivers[irq].name, tasking_get_current_task()->name);
    }
    assert(_adi_drivers[irq].task == tasking_get_current_task(), "Current task does not match driver layout");

    adi_driver_t* driver = _adi_drivers + irq;
    // If the driver has at least one interrupt to service now, don't block
    if (driver->pending_irq_count) {
        spinlock_release(&s);
        return true;
    }
    
    // If the driver has at least one message to service now, don't block
    if (amc_service_has_message(amc_service_of_active_task())) {
        spinlock_release(&s);
        return false;
    }

    // The driver has re-entered its await-event loop
    // Await the next interrupt or amc message
    task_small_t* task = (task_small_t*)(driver->task);
    tasking_block_task(task, IRQ_WAIT | AMC_AWAIT_MESSAGE);

    task_state_t unblock_reason = task->blocked_info.unblock_reason;
    tasking_get_current_task()->blocked_info.unblock_reason = 0;
    // Make sure this was an event we're expecting
    if (unblock_reason != IRQ_WAIT && unblock_reason != AMC_AWAIT_MESSAGE) {
        printf("Unknown awake reason: %d\n", unblock_reason);
    }
    assert(unblock_reason == IRQ_WAIT || unblock_reason == AMC_AWAIT_MESSAGE, "ADI driver awoke for unknown reason");

    spinlock_release(&s);
    return unblock_reason == IRQ_WAIT;
}

void adi_send_eoi(uint32_t irq) {
    adi_driver_t* driver = _adi_drivers + irq;
    driver->pending_irq_count -= 1;
    pic_signal_end_of_interrupt(irq);
}

bool adi_services_interrupt(uint32_t irq) {
    assert(irq > 0 && irq < MAX_INT_VECTOR, "Invalid IRQ provided");
    return _adi_drivers[irq].task != NULL;
}
