#ifndef PCI_MESSAGES_H
#define PCI_MESSAGES_H

#include <stdint.h>

#define PCI_SERVICE_NAME "com.axle.pci_driver"

// Sent from client to pci_driver as amc_msg_u32_5
// (Message ID, Bus, Device, Function, Offset)
#define PCI_REQUEST_READ_CONFIG_WORD (1 << 0)
// Sent from pci_driver to client as amc_msg_u32_2
// (Message ID, config word value)
#define PCI_RESPONSE_READ_CONFIG_WORD (1 << 0)

#define PCI_REQUEST_WRITE_CONFIG_WORD (1 << 1)
#define PCI_RESPONSE_WRITE_CONFIG_WORD (1 << 1)

#endif
