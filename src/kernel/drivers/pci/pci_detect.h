#ifndef PCI_DETECT_H
#define PCI_DETECT_H

#include <std/std.h>

typedef struct pci_device {
	uint16_t vendor;
	uint16_t device;
	uint16_t func;
} pci_device;

void pci_install(void);

#endif
