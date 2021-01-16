#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <kernel/adi.h>
#include <kernel/amc.h>
#include <kernel/idt.h>

// Layers and drawing
#include <agx/lib/size.h>
#include <agx/lib/screen.h>
#include <agx/lib/shapes.h>
#include <agx/lib/ca_layer.h>
#include <agx/lib/putpixel.h>
#include <agx/lib/text_box.h>

// Window management
#include <awm/awm.h>

// Communication with other processes
#include <libamc/libamc.h>

#include "pci_driver.h"

// Many graphics lib functions call gfx_screen() 
Screen _screen = {0};
Screen* gfx_screen() {
	return &_screen;
}

static ca_layer* window_layer_get(uint32_t width, uint32_t height) {
	// Ask awm to make a window for us
	amc_msg_u32_3__send("com.axle.awm", AWM_REQUEST_WINDOW_FRAMEBUFFER, width, height);
	// And get back info about the window it made
	amc_command_ptr_message_t receive_framebuf = {0};
	amc_message_await("com.axle.awm", &receive_framebuf);
	// TODO(PT): Need a struct type selector
	if (amc_command_ptr_msg__get_command(&receive_framebuf) != AWM_CREATED_WINDOW_FRAMEBUFFER) {
		printf("Invalid state. Expected framebuffer command\n");
	}

	printf("Received framebuffer from awm: %d 0x%08x\n", amc_command_ptr_msg__get_command(&receive_framebuf), amc_command_ptr_msg__get_ptr(&receive_framebuf));
	uint32_t framebuffer_addr = receive_framebuf.body.cmd_ptr.ptr_val;
	uint8_t* buf = (uint8_t*)framebuffer_addr;

	// TODO(PT): Use an awm command to get screen info
	_screen.resolution = size_make(1920, 1080);
	_screen.physbase = (uint32_t*)0;
	_screen.bits_per_pixel = 32;
	_screen.bytes_per_pixel = 4;

	ca_layer* dummy_layer = malloc(sizeof(ca_layer));
	memset(dummy_layer, 0, sizeof(dummy_layer));
	dummy_layer->size = _screen.resolution;
	dummy_layer->raw = (uint8_t*)framebuffer_addr;
	dummy_layer->alpha = 1.0;
    _screen.vmem = dummy_layer;

	return dummy_layer;
}

// Device IDs https://github.com/qemu/qemu/blob/master/include/hw/pci/pci_ids.h
// https://github.com/qemu/qemu/blob/master/docs/specs/pci-ids.txt
// Device class and subclass http://my.execpc.com/~geezer/code/pci.c

// TODO(PT): assert should be provided by a library
static void assert(bool cond, const char* msg) {
	if (!cond) {
		printf("Assertion failed: %s\n", msg);
		exit(1);
	}
}

void outb(uint16_t port, uint8_t val) {
	 asm volatile("outb %0, %1" : : "a"(val), "Nd"(port) );
}

void outw(uint16_t port, uint16_t val) {
	asm volatile("outw %0, %1" : : "a"(val), "dN"(port));
}

void outl(uint16_t port, uint32_t val) {
	asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

uint8_t inb(uint16_t port) {
	uint8_t _v;
	__asm__ __volatile__ ("inb %w1,%0":"=a" (_v):"Nd" (port));
	return _v;
}

uint16_t inw(uint16_t port) {
	uint16_t _v;
	__asm__ __volatile__ ("inw %1, %0" : "=a" (_v) : "dN" (port));
	return _v;
}

uint32_t inl(uint16_t port) {
	uint32_t _v;
	__asm __volatile__("inl %1, %0" : "=a" (_v) : "Nd" (port));
	return _v;
}

uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    // https://wiki.osdev.org/Pci#Enumerating_PCI_Buses
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16_t tmp = 0;
 
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
    return (uint16_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xffff);
}

void pci_config_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    // https://wiki.osdev.org/Pci#Enumerating_PCI_Buses
    // http://www.jbox.dk/sanos/source/sys/krnl/pci.c.html
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16_t tmp = 0;
 
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

const char* pci_vendor_name_for_id(uint16_t vendor_id) {
    switch (vendor_id) {
        case PCI_VENDOR_INTEL:
            return "Intel";
        case PCI_VENDOR_QEMU:
            return "Qemu";
        case PCI_VENDOR_REALTEK:
            return "RealTek";
        case PCI_VENDOR_NONE:
            return "No vendor/device";
        default:
            return "Unknown vendor";
    }
}

const char* pci_device_name_for_id(uint16_t device_id) {
    switch (device_id) {
        case PCI_DEVICE_ID_INTEL_82441:
            return "441FX Host Bridge";
        case PCI_DEVICE_ID_INTEL_82371SB_0:
            return "PIIX4 ISA Bridge";
        case PCI_DEVICE_ID_INTEL_82371SB_1:
            return "PIIX4 IDE Controller";
        case PCI_DEVICE_ID_INTEL_82371AB_3:
            // Taken from http://web.mit.edu/~linux/devel/redhat/Attic/6.0/src/pci-probing/foo
            return "PIIX4 ACPI";
        case PCI_DEVICE_ID_QEMU_VGA:
            return "StdVGA";
        case PCI_DEVICE_ID_REALTEK_8139:
            return "RTL8139";
        case PCI_DEVICE_ID_NONE:
            return "No Device";
        default:
            return "Unknown device";
    }
}

const char* pci_device_class_name(uint8_t device_class) {
    switch (device_class) {
        case PCI_DEVICE_CLASS_DISK_CONTROLLER:
            return "Disk Controller";
        case PCI_DEVICE_CLASS_NETWORK_CONTROLLER:
            return "Network Controller";
        case PCI_DEVICE_CLASS_DISPLAY_CONTROLLER:
            return "Display Controller";
        case PCI_DEVICE_CLASS_BRIDGE:
            return "Bridge";
        default:
            return "Unknown device class";
    }
}

const char* pci_device_subclass_name(uint8_t device_class, uint8_t device_subclass) {
    if (device_class == PCI_DEVICE_CLASS_DISK_CONTROLLER) {
        switch (device_subclass) {
            case PCI_DEVICE_SUBCLASS_DISK_CONTROLLER_IDE:
                return "IDE";
            default:
                break;
        }
    }
    else if (device_class == PCI_DEVICE_CLASS_NETWORK_CONTROLLER) {
        switch (device_subclass) {
            case PCI_DEVICE_SUBCLASS_NETWORK_CONTROLLER_ETHERNET:
                return "Ethernet";
            default:
                break;
        }
    }
    else if (device_class == PCI_DEVICE_CLASS_DISPLAY_CONTROLLER) {
        switch (device_subclass) {
            case PCI_DEVICE_SUBCLASS_DISPLAY_CONTROLLER_VGA:
                return "VGA";
            default:
                break;
        }
    }
    else if (device_class == PCI_DEVICE_CLASS_BRIDGE) {
        switch (device_subclass) {
            case PCI_DEVICE_SUBCLASS_BRIDGE_CPU:
                return "CPU";
            case PCI_DEVICE_SUBCLASS_BRIDGE_ISA:
                return "ISA";
            case PCI_DEVICE_SUBCLASS_BRIDGE_OTHER:
                return "Other";
            default:
                break;
        }
    }
    assert(0, "Unknown PCI device class/subclass combo");
    return NULL;
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
            uint16_t vendor_id = pci_config_read_word(bus, device_slot, 0, 0);
            if (vendor_id == PCI_VENDOR_NONE) {
                continue;
            }

            uint16_t tmp = pci_config_read_word(bus, device_slot, 0, 0x0e);
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
                uint16_t device_id = pci_config_read_word(bus, device_slot, function, 2);
                // Did we find a device?
                if (device_id == PCI_DEVICE_ID_NONE) {
                    // Function 0 should always work
                    assert(function != 0, "PCI function zero reported no device, which is not allowed");
                    // Skip this function
                    continue;
                }
                const char* vendor_name = pci_vendor_name_for_id(vendor_id);
                const char* device_name = pci_device_name_for_id(device_id);

                tmp = pci_config_read_word(bus, device_slot, function, 0x0a);
                uint8_t device_class = (tmp >> 8) & 0xff;
                uint8_t device_subclass = tmp & 0xff;
                const char* device_class_name = pci_device_class_name(device_class);
                const char* device_subclass_name = pci_device_subclass_name(device_class, device_subclass);

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

/*
#define PCI_BASE_ADDRESS_REGISTER_COUNT 7
for (int bar_idx = 0; bar_idx < PCI_BASE_ADDRESS_REGISTER_COUNT; bar_idx++) {
    // https://forum.osdev.org/viewtopic.php?t=11501
    uint32_t bar_offset = 0x10 + (bar_idx * sizeof(uint32_t));
    uint32_t base_address_register = pci_config_read_word(bus, device_slot, function, bar_offset);
    // Is there a Base Address Register (BAR) implemented?
    if (base_address_register == 0) {
        continue;
    }
    printf("\t\tBAR %d: 0x%08x\n", bar_idx, base_address_register);
    // For BARs, the first bit determines if the BAR is for a range of I/O ports 
    // or a range of addresses in the physical address space
    if ((base_address_register & 1) == 0) {
        // Memory
        uint32_t type = (base_address_register >> 1) & 0x03;
        if( (type & 2) == 0) {
            // 32 bit memory
            printf("\t\t32-bit memory\n");
            asm("cli");
            pci_write_dword(bus, device_slot, function, bar_offset, 0xFFFFFFFF);
            uint32_t bar_response = pci_config_read_word(bus, device_slot, function, bar_offset);
            // Clear flags
            bar_response = bar_response & 0xFFFFFFF0;          // Clear flags
            uint32_t size = (bar_response ^ 0xFFFFFFFF) + 1;
            // Rewrite the original BAR
            pci_write_dword(bus, device_slot, function, bar_offset, base_address_register);
            asm("sti");
            printf("\t\tPCI BAR %d:%d:%d offset %d = 0x%08x bytes (32 bit)\n", bus, device_slot, function, bar_offset, size);
        }
        else {
            // 64 bit memory
            printf("\t\t64-bit memory\n");
            NotImplemented();
        }
    }
    else {
        // IO space
        printf("\t\tIO Space\n");
        asm("cli");
        pci_write_dword(bus, device_slot, function, bar_offset, 0xFFFFFFFF);
        uint32_t bar_response = pci_config_read_word(bus, device_slot, function, bar_offset);
        bar_response = bar_response & 0xFFFFFFFC;          // Clear flags
        uint32_t size = (bar_response ^ 0xFFFFFFFF) + 1;
        // Rewrite the original BAR
        pci_write_dword(bus, device_slot, function, bar_offset, base_address_register);
        asm("sti");
        printf("\t\t\tBAR cleared 0x%08x\n", bar_response);
        printf("\t\tBAR offset %d = 0x%08x I/O ports\n", bar_offset, size);

        if (device_id == PCI_DEVICE_ID_REALTEK_8139) {
            realtek_8139_init(bus, device_slot, function, bar_response);
            break;
        }
    }
}
*/

int main(int argc, char** argv) {
	amc_register_service("com.axle.pci_driver");

	Size window_size = size_make(500, 460);
	Rect window_frame = rect_make(point_zero(), window_size);
	ca_layer* window_layer = window_layer_get(window_size.width, window_size.height);

	int text_box_padding = 6;
	Rect text_box_frame = rect_make(
		point_make(
			text_box_padding,
			text_box_padding
		),
		size_make(
			window_size.width - (text_box_padding * 2), 
			window_size.height - (text_box_padding * 2)
		)
	);
	draw_rect(window_layer, window_frame, color_light_gray(), THICKNESS_FILLED);
	text_box_t* text_box = text_box_create(text_box_frame.size, color_dark_gray());

    // Perform PCI scan
    pci_dev_t* dev = pci_find_devices();
    pci_dev_t* dev_head = dev;
    // Iterate the PCI devices and draw them into the text box
    Color text_color = color_green();
    while (dev != NULL) {
        const char buf[256];
        snprintf(buf, sizeof(buf), "%s %s\n", dev->vendor_name, dev->device_name);
        text_box_puts(text_box, buf, text_color);
        snprintf(buf, sizeof(buf), "\t%s %s\n", dev->device_subclass_name, dev->device_class_name);
        text_box_puts(text_box, buf, text_color);
        snprintf(buf, sizeof(buf), "\tBFD %d:%d:%d, ID %04x:%04x\n", dev->bus, dev->device_slot, dev->function, dev->vendor_id, dev->device_id);
        text_box_puts(text_box, buf, text_color);
        text_box_puts(text_box, "\n\n", text_color);

        dev = dev->next;
    }

    // Blit the text box to the window layer
    blit_layer(window_layer, text_box->layer, text_box_frame, rect_make(point_zero(), text_box_frame.size));
    // And ask awm to draw our window
    amc_command_msg__send("com.axle.awm", AWM_WINDOW_REDRAW_READY);

    // Launch drivers for known devices
    dev = dev_head;
    while (dev != NULL) {
        if (dev->device_id == PCI_DEVICE_ID_REALTEK_8139) {
            printf("[PCI] Launching driver for %s %s\n", dev->vendor_id, dev->device_name);
            // TODO(PT): This should be done via an amc interface
            uint32_t command_register = pci_config_read_word(dev->bus, dev->device_slot, dev->function, 0x04);
            printf("CmdReg before enable 0x%08x\n", command_register);
            // Enable bus mastering bit
            command_register |= (1 << 2);
            printf("CmdReg after enable 0x%08x\n", command_register);
            pci_config_write_word(dev->bus, dev->device_slot, dev->function, 0x04, command_register);

            amc_launch_service("com.axle.realtek_8139");
        }
        dev = dev->next;
    }
    printf("Awaiting next message\nO");

	while (true) {
		amc_charlist_message_t msg = {0};
		do {
			// Wait until we've unblocked with at least one message available
			amc_message_await_any(&msg);
			// TODO(PT): Process the message
		} while (amc_has_message());
		// We're out of messages to process
		// Wait for a new message to arrive on the next loop iteration
	}

	return 0;
}
