#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <kernel/adi.h>
#include <kernel/amc.h>
#include <kernel/idt.h>

#include <libgui/libgui.h>

// Communication with other processes
#include <libamc/libamc.h>

#include <libport/libport.h>
#include <libutils/assert.h>

#include "pci_driver.h"
#include "pci_messages.h"

uint32_t pci_config_read_u32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    // https://wiki.osdev.org/Pci#Enumerating_PCI_Buses
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
 
    // Construct an address as per the PCI "Configuration Space Access Mechanism #1"
    uint32_t address = (uint32_t)((lbus << 16) | 
								  (lslot << 11) |
								  (lfunc << 8) | 
								  (offset & 0xfc) | 
								  ((uint32_t)0x80000000));
 
	// Write out the address
    outl(PCI_CONFIG_ADDRESS_PORT, address);
	// Read in the data
    // (offset & 2) * 8) = 0 will choose the first word of the 32 bits register
    return inl(0xCFC);
}

uint16_t pci_config_read_u16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t response = pci_config_read_u32(bus, slot, func, offset);
    // (offset & 2) * 8) = 0 will choose the first word of the 32 bits register
    return (uint16_t)((response >> ((offset & 2) * 8)) & 0xffff);
}

void pci_config_write_u32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    // https://wiki.osdev.org/Pci#Enumerating_PCI_Buses
    // http://www.jbox.dk/sanos/source/sys/krnl/pci.c.html
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
 
    // Construct an address as per the PCI "Configuration Space Access Mechanism #1"
    uint32_t address = (uint32_t)((lbus << 16) | 
								  (lslot << 11) |
								  (lfunc << 8) | 
								  (offset & 0xfc) | 
								  ((uint32_t)0x80000000));

	// Write out the address
    outl(PCI_CONFIG_ADDRESS_PORT, address);
    outl(0xCFC, value);
}

void pci_find_device_class_and_subclass_names(uint8_t device_class_id, uint8_t device_subclass_id, const char** out_device_class_name, const char** out_device_subclass_name) {
    *out_device_class_name = "Unknown";
    *out_device_subclass_name = "Unknown";
    for (uint32_t i = 0; i < sizeof(pci_device_classes) / sizeof(pci_device_classes[0]); i++) {
        pci_device_class_info_t* device_class = &pci_device_classes[i];
        if (device_class->device_class_id == device_class_id) {
            *out_device_class_name = device_class->name;
            for (uint32_t j = 0; j < PCI_MAX_SUBCLASSES_PER_DEVICE_CLASS; j++) {
                pci_device_subclass_info_t* subclass = &device_class->subclasses[j];
                if (subclass->subclass_id == device_subclass_id) {
                    *out_device_subclass_name = subclass->name;
                    break;
                }
            }
            return;
        }
    }
}

void pci_find_vendor_and_device_names(uint16_t vendor_id, uint16_t device_id, const char** out_vendor_name, const char** out_device_name) {
    *out_vendor_name = "Unknown";
    *out_device_name = "Unknown";
    for (uint32_t i = 0; i < sizeof(pci_vendors) / sizeof(pci_vendors[0]); i++) {
        pci_vendor_info_t* vendor = &pci_vendors[i];
        if (vendor->vendor_id == vendor_id) {
            *out_vendor_name = vendor->name;
            for (uint32_t j = 0; j < PCI_MAX_DEVICES_PER_VENDOR; j++) {
                pci_device_info_t* device = &vendor->devices[j];
                if (device->device_id == device_id) {
                    *out_device_name = device->name;
                    break;
                }
            }
            return;
        }
    }
}

typedef struct pci_dev {
	// Location within the PCI subsystem
	uint8_t bus;
	uint8_t device_slot;
	uint8_t function;

	// General device category and functionality
	uint8_t device_class;
	uint8_t device_subclass;
	const char* device_class_name;
	const char* device_subclass_name;

	// Specific manufacturer/model info
	uint16_t vendor_id;
	uint16_t device_id;
	const char* vendor_name;
	const char* device_name;

	// Next PCI device in the linked list
	struct pci_dev* next;
} pci_dev_t;

static pci_dev_t* pci_find_devices() {
    // https://forum.osdev.org/viewtopic.php?f=1&t=30546
    // https://gist.github.com/extremecoders-re/e8fd8a67a515fee0c873dcafc81d811c
    // https://www.qemu.org/2018/05/31/nic-parameter/
	pci_dev_t* dev_head = NULL;
	pci_dev_t* prev_dev = NULL;

    for (int bus = 0; bus < 256; bus++) {
        for (int device_slot = 0; device_slot < 32; device_slot++) {
            // Is there a device plugged into this slot?
            uint16_t vendor_id = pci_config_read_u16(bus, device_slot, 0, 0);
            if (vendor_id == PCI_VENDOR_NONE) {
                continue;
            }

            uint16_t tmp = pci_config_read_u16(bus, device_slot, 0, 0x0e);
            uint8_t header_type = tmp & 0xff;

            // Every PCI device is required to at least provide function "0"
            uint8_t function_count_to_poll = 1;
            // If the high bit of the header type is set, the device supports multiple functions
            // But we don't know what functions exactly are supported - we must poll each one
            // https://forum.osdev.org/viewtopic.php?t=9987
            if ((header_type >> 7) & 0x1) {
                function_count_to_poll = 8;
            }

            for (int function = 0; function < function_count_to_poll; function++) {
                uint16_t device_id = pci_config_read_u16(bus, device_slot, function, 2);
                // Did we find a device?
                if (device_id == PCI_DEVICE_ID_NONE) {
                    // Function 0 should always work
                    assert(function != 0, "PCI function zero reported no device, which is not allowed");
                    // Skip this function
                    continue;
                }
                const char* vendor_name = NULL;
                const char* device_name = NULL;
                pci_find_vendor_and_device_names(vendor_id, device_id, &vendor_name, &device_name);

                tmp = pci_config_read_u16(bus, device_slot, function, 0x0a);
                uint8_t device_class = (tmp >> 8) & 0xff;
                uint8_t device_subclass = tmp & 0xff;
                const char* device_class_name = NULL;
                const char* device_subclass_name = NULL;
                pci_find_device_class_and_subclass_names(device_class, device_subclass, &device_class_name, &device_subclass_name);

				// We've collected all the information we need to construct the `pci_dev_t` structure
				pci_dev_t* current_dev = calloc(1, sizeof(pci_dev_t));
				// Is this the first device we've found?
				if (prev_dev == NULL) {
                    dev_head = current_dev;
				}
				else {
                    prev_dev->next = current_dev;
				}

				current_dev->bus = bus;
				current_dev->device_slot = device_slot;
				current_dev->function = function;
				current_dev->device_class = device_class;
				current_dev->device_subclass = device_subclass;
				// TODO(PT): Perhaps the *_name fields should be moved out of the representation
				current_dev->device_class_name  = device_class_name;
				current_dev->device_subclass_name  = device_subclass_name;

				current_dev->vendor_id  = vendor_id;
				current_dev->device_id  = device_id;
				current_dev->vendor_name  = vendor_name;
				current_dev->device_name  = device_name;

				prev_dev = current_dev;
            }
        }
    }

	// Did we fail to find any PCI devices?
	assert(dev_head != NULL, "Failed to find at least 1 PCI device");
	return dev_head;
}

bool is_service_pci_device_driver(const char* service_name) {
    if (!strcmp(service_name, "com.axle.realtek_8139_driver")) {
        return true;
    }
    return false;
}

static void launch_known_drivers(pci_dev_t* dev_head) {
    pci_dev_t* dev = dev_head;
    while (dev != NULL) {
        if (dev->device_id == PCI_DEVICE_ID__REALTEK__8139) {
            printf("[PCI] Launching driver for %s %s\n", dev->vendor_name, dev->device_name);
            // TODO(PT): Revisit
            //amc_launch_service("com.axle.realtek_8139_driver");
        }
        dev = dev->next;
    }
}

static Rect _info_text_view_sizer(gui_text_view_t* tv, Size window_size) {
	return rect_make(point_zero(), window_size);
}

static void _handle_amc_message(amc_message_t* msg) {
    const char* source_service = msg->source;
    // If we're sent a message from someone other than a PCI device driver, ignore it
    /*
    if (!is_service_pci_device_driver(source_service)) {
        return;
    }
    */
    
    uint32_t message_id = amc_msg_u32_get_word(msg, 0);
    printf("PCI request from %s: %d\n", source_service, message_id);
    if (message_id == PCI_REQUEST_READ_CONFIG_WORD) {
        uint32_t bus = amc_msg_u32_get_word(msg, 1);
        uint32_t device_slot = amc_msg_u32_get_word(msg, 2);
        uint32_t function = amc_msg_u32_get_word(msg, 3);
        uint32_t config_word_offset = amc_msg_u32_get_word(msg, 4);
        printf("Request to get config word [%ld,%ld,%ld] @ %ld\n", bus, device_slot, function, config_word_offset);
        uint32_t config_word = (uint32_t)pci_config_read_u32(bus, device_slot, function, config_word_offset);
        amc_msg_u32_2__send(source_service, PCI_RESPONSE_READ_CONFIG_WORD, config_word);
    }
    else if (message_id == PCI_REQUEST_WRITE_CONFIG_WORD) {
        uint32_t bus = amc_msg_u32_get_word(msg, 1);
        uint32_t device_slot = amc_msg_u32_get_word(msg, 2);
        uint32_t function = amc_msg_u32_get_word(msg, 3);
        uint32_t config_word_offset = amc_msg_u32_get_word(msg, 4);
        uint32_t new_value = amc_msg_u32_get_word(msg, 5);
        printf("Request to write config word [%ld,%ld,%ld] @ %ld to 0x%08lx\n", bus, device_slot, function, config_word_offset, new_value);
        pci_config_write_u32(bus, device_slot, function, config_word_offset, new_value);
        amc_msg_u32_1__send(source_service, PCI_RESPONSE_WRITE_CONFIG_WORD);
    }
}

int main(int argc, char** argv) {
	amc_register_service(PCI_SERVICE_NAME);

    gui_window_t* window = gui_window_create("Connected PCI Devices", 400, 620);
    gui_text_view_t* info_text_view = gui_text_view_create(
        window,
        (gui_window_resized_cb_t)_info_text_view_sizer
    );

    // Perform PCI scan
    pci_dev_t* dev = pci_find_devices();
    pci_dev_t* dev_head = dev;
    // Iterate the PCI devices and draw them into the text box
    Color text_color = color_green();
    while (dev != NULL) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s %s\n", dev->vendor_name, dev->device_name);
        gui_text_view_puts(info_text_view, buf, text_color);
        snprintf(buf, sizeof(buf), "\t%s %s\n", dev->device_subclass_name, dev->device_class_name);
        gui_text_view_puts(info_text_view, buf, text_color);
        snprintf(buf, sizeof(buf), "\tBFD %d:%d:%d, ID %04x:%04x\n", dev->bus, dev->device_slot, dev->function, dev->vendor_id, dev->device_id);
        gui_text_view_puts(info_text_view, buf, text_color);
        gui_text_view_puts(info_text_view, "\n\n", text_color);

        dev = dev->next;
    }

    // Launch drivers for known devices
    launch_known_drivers(dev_head);

    gui_add_message_handler(_handle_amc_message);
    gui_enter_event_loop();

	return 0;
}
