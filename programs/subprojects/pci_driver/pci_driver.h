#ifndef PCI_DRIVER_H
#define PCI_DRIVER_H

#include <stdint.h>

#define PCI_CONFIG_ADDRESS_PORT 0xCF8
#define PCI_CONFIG_DATA_PORT    0xCFC

#define PCI_DEVICE_HEADER_TYPE_DEVICE               0x00
#define PCI_DEVICE_HEADER_TYPE_PCI_TO_PCI_BRIDGE    0x01
#define PCI_DEVICE_HEADER_TYPE_CARDBUS_BRIDGE       0x02

#define PCI_VENDOR_NONE         0xFFFF
#define PCI_DEVICE_ID_NONE              0xFFFF
// Devices infos that we need to reference e.g. to launch a driver
#define PCI_DEVICE_ID__REALTEK__8139 0x8139

#define PCI_MAX_SUBCLASSES_PER_DEVICE_CLASS 32

typedef struct pci_device_subclass_info {
    uint8_t subclass_id;
    const char* name;
} pci_device_subclass_info_t;

typedef struct pci_device_class_info {
    uint8_t device_class_id;
    const char* name;
    pci_device_subclass_info_t subclasses[PCI_MAX_SUBCLASSES_PER_DEVICE_CLASS];
} pci_device_class_info_t;

#define PCI_MAX_DEVICES_PER_VENDOR 32

typedef struct pci_device_info {
    uint16_t device_id;
    const char* name;
} pci_device_info_t;

typedef struct pci_vendor_info {
    uint16_t vendor_id;
    const char* name;
    pci_device_info_t devices[PCI_MAX_DEVICES_PER_VENDOR];
} pci_vendor_info_t;

// https://pci-ids.ucw.cz/read/PD/
// http://my.execpc.com/~geezer/code/pci.c
pci_device_class_info_t pci_device_classes[] = {
    {
        .device_class_id = 0x00,
        .name = "Unclassified",
        .subclasses = {}
    },
    {
        .device_class_id = 0x01,
        .name = "Disk Controller",
        .subclasses = {
            {
                .subclass_id = 0x01,
                .name = "IDE"
            },
            {
                .subclass_id = 0x06,
                .name = "SATA"
            }
        }
    },
    {
        .device_class_id = 0x02,
        .name = "Network Controller",
        .subclasses = {
            {
                .subclass_id = 0x00,
                .name = "Ethernet"
            }
        }
    },
    {
        .device_class_id = 0x03,
        .name = "Display Controller",
        .subclasses = {
            {
                .subclass_id = 0x00,
                .name = "VGA"
            }
        }
    },
    {
        .device_class_id = 0x04,
        .name = "Multimedia Controller",
        .subclasses = {
            {
                .subclass_id = 0x03,
                .name = "Audio"
            },
        }
    },
    {
        .device_class_id = 0x05,
        .name = "Memory Controller",
        .subclasses = {}
    },
    {
        .device_class_id = 0x06,
        .name = "Bridge",
        .subclasses = {
            {
                .subclass_id = 0x00,
                .name = "CPU"
            },
            {
                .subclass_id = 0x01,
                .name = "ISA"
            },
            {
                .subclass_id = 0x04,
                .name = "PCI"
            },
            {
                .subclass_id = 0x80,
                .name = "Generic"
            }
        }
    },
    {
        .device_class_id = 0x08,
        .name = "System Device",
        .subclasses = {
            {
                .subclass_id = 0x06,
                .name = "IOMMU"
            }
        }
    },
    {
        .device_class_id = 0x0C,
        .name = "Serial Bus Controller",
        .subclasses = {
            {
                .subclass_id = 0x03,
                .name = "USB"
            },
            {
                .subclass_id = 0x05,
                .name = "SMBus"
            }
        }
    },
    {
        .device_class_id = 0x10,
        .name = "Encryption Controller",
        .subclasses = {
            {
                .subclass_id = 0x80,
                .name = "Generic"
            }
        }
    },
    {
        .device_class_id = 0x13,
        .name = "Non-Essential Instrumentation",
        .subclasses = {}
    },
    {
        .device_class_id = 0xFF,
        .name = "Unassigned",
        .subclasses = {}
    },
};

// https://pcilookup.com
// https://linux-hardware.org
// https://gist.github.com/cuteribs/0a4d85f745506c801d46bea22b554f7d
// http://web.mit.edu/~linux/devel/redhat/Attic/6.0/src/pci-probing/foo
// https://github.com/qemu/qemu/blob/master/include/hw/pci/pci_ids.h
// https://github.com/qemu/qemu/blob/master/docs/specs/pci-ids.txt
pci_vendor_info_t pci_vendors[] = {
    {
        .name = "None",
        .vendor_id = PCI_VENDOR_NONE,
        .devices = {
            {
                .device_id = PCI_DEVICE_ID_NONE,
                .name = "None"
            }
        },
    },
    {
        .name = "Intel",
        .vendor_id = 0x8086,
        .devices = {
            {
                .device_id = 0x100E,
                .name = "82540EM Gigabit Ethernet Controller"
            },
            {
                .device_id = 0x1237,
                .name = "440FX Host Bridge"
            },
            {
                .device_id = 0x7000,
                .name = "PIIX3 ISA Bridge"
            },
            {
                .device_id = 0x7010,
                .name = "PIIX3 IDE Controller"
            },
            {
                .device_id = 0x7113,
                .name = "PIIX4 ACPI"
            },
            {
                .device_id = 0x2922,
                .name = "82801IR/IO/IH (ICH9R/DO/DH) 6 port SATA Controller [AHCI mode]"
            },
        }
    },
    {
        .name = "RealTek",
        .vendor_id = 0x10EC,
        .devices = {
            {
                .device_id = PCI_DEVICE_ID__REALTEK__8139,
                .name = "RTL-8100/8101L/8139 PCI Fast Ethernet Adapter"
            },
            {
                .device_id = 0x8168,
                .name = "RTL8111/8168/8411 PCI Express Gigabit Ethernet Controller"
            },
        }
    },
    {
        .name = "QEMU",
        .vendor_id = 0x1234,
        .devices = {
            {
                .device_id = 0x1111,
                .name = "Paravirtualized VGA"
            }
        }
    },
    {
        .name = "NVIDIA",
        .vendor_id = 0x10DE,
        .devices = {
            {
                .device_id = 0x13C0,
                .name = "GeForce GTX 980"
            },
            {
                .device_id = 0xFBB,
                .name = "GM204 HD Audio Controller"
            }
        }
    },
    {
        .name = "AMD",
        .vendor_id = 0x1022,
        .devices = {
            {
                .device_id = 0x1480,
                .name = "Starship/Matisse Root Complex"
            },
            {
                .device_id = 0x1481,
                .name = "Starship/Matisse IOMMU"
            },
            {
                .device_id = 0x1482,
                .name = "Starship/Matisse PCIe Dummy Host Bridge"
            },
            {
                .device_id = 0x1483,
                .name = "Starship/Matisse GPP Bridge"
            },
            {
                .device_id = 0x1484,
                .name = "Starship/Matisse Internal PCIe GPP Bridge 0 to bus[E:B]"
            },
            {
                .device_id = 0x1485,
                .name = "Starship/Matisse Reserved SPP"
            },
            {
                .device_id = 0x1486,
                .name = "Starship/Matisse Cryptographic Coprocessor PSPCPP"
            },
            {
                .device_id = 0x1487,
                .name = "Starship/Matisse HD Audio Controller"
            },
            {
                .device_id = 0x148A,
                .name = "Starship/Matisse PCIe Dummy Function"
            },
            {
                .device_id = 0x149C,
                .name = "Matisse USB 3.0 Host Controller"
            },
            {
                .device_id = 0x790B,
                .name = "FCH SMBus Controller"
            },
            {
                .device_id = 0x790E,
                .name = "FCH LPC Bridge"
            },
            {
                .device_id = 0x1440,
                .name = "Matisse Device 24: Function 0"
            },
            {
                .device_id = 0x1441,
                .name = "Matisse Device 24: Function 1"
            },
            {
                .device_id = 0x1442,
                .name = "Matisse Device 24: Function 2"
            },
            {
                .device_id = 0x1443,
                .name = "Matisse Device 24: Function 3"
            },
            {
                .device_id = 0x1444,
                .name = "Matisse Device 24: Function 4"
            },
            {
                .device_id = 0x1445,
                .name = "Matisse Device 24: Function 5"
            },
            {
                .device_id = 0x1446,
                .name = "Matisse Device 24: Function 6"
            },
            {
                .device_id = 0x1447,
                .name = "Matisse Device 24: Function 7"
            },
            {
                .device_id = 0x43EE,
                .name = "Starship/Matisse USB 3.0 Host Controller"
            },
            {
                .device_id = 0x43EB,
                .name = "Starship/Matisse Chipset SATA Controller [AHCI mode]"
            },
            {
                .device_id = 0x43E9,
                .name = "Starship/Matisse PCIe Upstream Switch Port"
            },
            {
                .device_id = 0x43EA,
                .name = "Starship/Matisse PCIe Downstream Switch Port"
            },
        }
    },
    {
        .name = "VMWare",
        .vendor_id = 0x15AD,
        .devices = {
            {
                .device_id = 0x0405,
                .name = "SVGA II Adapter"
            },
        }
    },
    {
        .name = "Red Hat",
        .vendor_id = 0x1B36,
        .devices = {
            {
                .device_id = 0x000D,
                .name = "QEMU XHCI Host Controller"
            },
        }
    },
};

#endif
