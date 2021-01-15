#ifndef ADI_H
#define ADI_H

#include <stdbool.h>
#include <stdint.h>

typedef struct adi_driver {
    uint32_t irq;
    // A driver may perform an action that causes its IRQ to be raised while it's running
    // (For example, the RTL8139 driver may send a packet, raising an IRQ quickly)
    // Keep track of how many IRQs are awaiting servicing by the driver
    uint32_t pending_irq_count;
    const char* name;
    void* task; // task_small_t
} adi_driver_t;

// ############
// Called externally by drivers to interface with adi
// ############

// Register the running process as the provided driver name
// This driver will be responsible for handling the provided IRQ
// The process's priority will be elevated to PRIORITY_DRIVER
void adi_register_driver(const char* name, uint32_t irq);

// Block until an event is received
// An event will be either an interrupt that must be serviced, or an amc message
// Returns true if the call returned due to an interrupt needing servicing,
// or false if the call returned due to an amc message arriving
bool adi_event_await(uint32_t irq);


// ############
// Called internally from kernel mode
// ############

// Called by an interrupt handler to wake up the driver process to service the interrupt
void adi_interrupt_dispatch(uint32_t irq);

// Returns whether there is a driver registered to handle the provided IRQ
bool adi_services_interrupt(uint32_t irq);

#endif