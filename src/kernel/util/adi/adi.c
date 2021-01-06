#include "adi.h"
#include <std/math.h>
#include <kernel/interrupts/pic.h>
#include <kernel/util/spinlock/spinlock.h>
#include <kernel/multitasking/tasks/task_small.h>

#define MAX_IRQ 32
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

void adi_register_driver(const char* name, uint32_t irq) {
    assert(irq > 0 && irq < MAX_INT_VECTOR, "Invalid IRQ provided");
    assert(!_adi_drivers[irq].task, "IRQ already mapped to a driver task");

    task_small_t* current_task = tasking_get_current_task();

    // Elevate the task's priority since it's a device driver
    spinlock_acquire(&current_task->priority_lock);
    current_task->priority = PRIORITY_DRIVER;
    spinlock_release(&current_task->priority_lock);

    _adi_drivers[irq].task = current_task;
    // The provided string is mapped into the address space of the running process,
    // but isn't mapped into kernel-space.
    // Copy the string so we can access it in kernel-space
    _adi_drivers[irq].name = strdup(name);
}

void adi_interrupt_await(uint32_t irq) {
    assert(irq > 0 && irq < MAX_INT_VECTOR, "Invalid IRQ provided");
    assert(_adi_drivers[irq].task == tasking_get_current_task(), "Current task does not match driver layout");

    adi_driver_t* driver = _adi_drivers + irq;
    // The driver has re-entered its await-interrupt loop
    // This means the IRQ has been serviced by the driver
    // Tell the PIC we're ready for more
    // Unless this is the first iteration of the event loop
    if (driver->int_count != 0) {
        pic_signal_end_of_interrupt(irq);
    }
    
    // The previous interrupt has now been fully serviced
    driver->int_count += 1;

    // Await the next interrupt
    tasking_block_task(driver->task, IRQ_WAIT);
}

void adi_interrupt_dispatch(uint32_t irq) {
    assert(irq > 0 && irq < MAX_INT_VECTOR, "Invalid IRQ provided");
    assert(_adi_drivers[irq].task != NULL, "IRQ does not have a corresponding driver");

    adi_driver_t* driver = _adi_drivers + irq;
    tasking_unblock_task(driver->task, false);
}

bool adi_services_interrupt(uint32_t irq) {
    assert(irq > 0 && irq < MAX_INT_VECTOR, "Invalid IRQ provided");
    return _adi_drivers[irq].task != NULL;
}
