#ifndef ADI_H
#define ADI_H

#include <stdbool.h>
#include <stdint.h>

typedef struct adi_driver {
    uint32_t irq;
    uint32_t int_count;
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
// Block until an interrupt is received
void adi_interrupt_await(uint32_t irq);

// ############
// Called internally from kernel mode
// ############

// Returns whether there is a driver registered to handle the provided IRQ
bool adi_services_interrupt(uint32_t irq);

#endif