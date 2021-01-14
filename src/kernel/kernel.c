//freestanding headers
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//kernel stdlib headers
#include <std/printf.h>
#include <std/string.h>

//kernel headers
#include <kernel/multiboot.h>
#include <kernel/boot.h>
#include <kernel/elf.h>
#include <kernel/assert.h>
#include <kernel/boot_info.h>
#include <kernel/segmentation/gdt.h>
#include <kernel/interrupts/interrupts.h>

//kernel drivers
#include <kernel/drivers/kb/kb.h>
#include <kernel/drivers/mouse/mouse.h>
#include <kernel/drivers/pit/pit.h>
#include <kernel/drivers/serial/serial.h>
#include <kernel/drivers/text_mode/text_mode.h>

//higher-level kernel features
#include <kernel/pmm/pmm.h>
#include <kernel/vmm/vmm.h>
#include <std/kheap.h>
#include <kernel/syscall/syscall.h>

//testing!
#include <kernel/multitasking/tasks/task_small.h>
#include <kernel/util/amc/amc.h>
#include <kernel/util/vfs/fs.h>

#define SPIN while (1) {sys_yield(RUNNABLE);}
#define SPIN_NOMULTI do {} while (1);

void print_os_name() {
    NotImplemented();
}

void system_mem() {
    NotImplemented();
}

static void kernel_spinloop() {
    printf("\nBoot complete, kernel spinlooping.\n");
    asm("cli");
    asm("hlt");
}

static void kernel_idle() {
    while (1) {
        //nothing to do!
        //put the CPU to sleep until the next interrupt
        asm volatile("hlt");
    }
}

static void awm_init() {
    const char* program_name = "awm";

    // VESA Framebuffer,
    boot_info_t* info = boot_info_get();
    vmm_identity_map_region(vmm_active_pdir(), info->framebuffer.address, info->framebuffer.size);

    FILE* fp = initrd_fopen(program_name, "rb");
    // Pass a pointer to the VESA linear framebuffer in argv
    // Not great, but kind of funny :-)
    // Later, framebuffer info can be communicated to awm via 
    // an init amc message.
    framebuffer_info_t framebuffer_info = boot_info_get()->framebuffer; 
    char* ptr = kmalloc(32);
    snprintf(ptr, 32, "0x%08x", &(boot_info_get()->framebuffer));
    char* argv[] = {program_name, ptr, NULL};

    elf_load_file(program_name, fp, argv);
    panic("noreturn");
}

static void cat() {
    const char* program_name = "cat";

    FILE* fp = initrd_fopen(program_name, "rb");
    char* argv[] = {program_name, "test-file.txt", NULL};
    elf_load_file(program_name, fp, argv);
    panic("noreturn");
}

static void rainbow() {
    const char* program_name = "rainbow";

    FILE* fp = initrd_fopen(program_name, "rb");
    char* argv[] = {program_name, NULL};
    elf_load_file(program_name, fp, argv);
    panic("noreturn");
}

static void paintbrush() {
    const char* program_name = "paintbrush";
    FILE* fp = initrd_fopen(program_name, "rb");
    char* argv[] = {program_name, NULL};
    elf_load_file(program_name, fp, argv);
    panic("noreturn");
}

static void textpad() {
    const char* program_name = "textpad";
    FILE* fp = initrd_fopen(program_name, "rb");
    char* argv[] = {program_name, NULL};
    elf_load_file(program_name, fp, argv);
    panic("noreturn");
}


static void tty_init() {
    const char* program_name = "tty";
    FILE* fp = initrd_fopen(program_name, "rb");
    char* argv[] = {program_name, NULL};
    elf_load_file(program_name, fp, argv);
	panic("noreturn");
	uint8_t scancode = inb(0x60);
}



#define PCI_CONFIG_ADDRESS_PORT 0xCF8
#define PCI_CONFIG_DATA_PORT    0xCFC

#define PCI_DEVICE_HEADER_TYPE_DEVICE               0x00
#define PCI_DEVICE_HEADER_TYPE_PCI_TO_PCI_BRIDGE    0x01
#define PCI_DEVICE_HEADER_TYPE_CARDBUS_BRIDGE       0x02

// https://gist.github.com/cuteribs/0a4d85f745506c801d46bea22b554f7d
#define PCI_VENDOR_NONE         0xFFFF
#define PCI_VENDOR_INTEL        0x8086
#define PCI_VENDOR_REALTEK      0x10EC
#define PCI_VENDOR_QEMU         0x1234

#define PCI_DEVICE_ID_INTEL_82441       0x1237
#define PCI_DEVICE_ID_INTEL_82371SB_0   0x7000
#define PCI_DEVICE_ID_INTEL_82371SB_1   0x7010
#define PCI_DEVICE_ID_INTEL_82371AB_3   0x7113
#define PCI_DEVICE_ID_QEMU_VGA          0x1111
#define PCI_DEVICE_ID_REALTEK_8139      0x8139
#define PCI_DEVICE_ID_NONE              0xFFFF

// http://my.execpc.com/~geezer/code/pci.c
#define PCI_DEVICE_CLASS_DISK_CONTROLLER    0x01
#define PCI_DEVICE_CLASS_NETWORK_CONTROLLER 0x02
#define PCI_DEVICE_CLASS_DISPLAY_CONTROLLER 0x03
#define PCI_DEVICE_CLASS_BRIDGE             0x06

#define PCI_DEVICE_SUBCLASS_DISK_CONTROLLER_IDE         0x01
#define PCI_DEVICE_SUBCLASS_NETWORK_CONTROLLER_ETHERNET 0x00
#define PCI_DEVICE_SUBCLASS_DISPLAY_CONTROLLER_VGA      0x00
#define PCI_DEVICE_SUBCLASS_BRIDGE_CPU                  0x00
#define PCI_DEVICE_SUBCLASS_BRIDGE_ISA                  0x01
#define PCI_DEVICE_SUBCLASS_BRIDGE_OTHER                0x80


// Device IDs https://github.com/qemu/qemu/blob/master/include/hw/pci/pci_ids.h
// https://github.com/qemu/qemu/blob/master/docs/specs/pci-ids.txt

uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    // https://wiki.osdev.org/Pci#Enumerating_PCI_Buses
    // Construct an address as per the PCI "Configuration Space Access Mechanism #1"
    /*
    uint32_t address = 0;
    // Set "enable" bit
    address |= 0x80000000;
    // Set bus number in bits 23 - 16
    address |= (uint32_t)(bus << 16);
    // Set slot number in bits 15-11
    address |= (uint32_t)(slot << 11);
    // Set function number in bits 10-8
    address |= (uint32_t)(func << 8);
    // Set register offset in bits 7-2
    // The register offset should always be 32-bit aligned
    // (There are 64 32-bit registers to address within the 256-byte space)
    address |= (uint32_t)offset & 0xfc;
    // (Bits 1-0 should be zero)

    // Write the request
    outl(PCI_CONFIG_ADDRESS_PORT, address);
    // Read in the returned data
	// (offset & 2) * 8) == 0 will choose first word of 32b register
	return (uint16_t)((inl(PCI_CONFIG_DATA_PORT) >> ((offset & 2) * 8)) & 0xFFFF);
    */
       uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16_t tmp = 0;
 
    /* create configuration address as per Figure 1 */
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
 
    /* write out the address */
    outl(0xCF8, address);
    /* read in the data */
    /* (offset & 2) * 8) = 0 will choose the first word of the 32 bits register */
    tmp = (uint16_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xffff);
    return (tmp);
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

static unsigned long pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, unsigned reg) {
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16_t tmp = 0;
 
    /* create configuration address as per Figure 1 */
    uint32_t address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (reg & 0xfc) | ((uint32_t)0x80000000));
	outl(PCI_CONFIG_ADDRESS_PORT, address);
	return inl(PCI_CONFIG_DATA_PORT);
}

static void pci_write_dword(int bus, int slot, int func, int addr, unsigned long value) {
    // http://www.jbox.dk/sanos/source/sys/krnl/pci.c.html
    outl(PCI_CONFIG_ADDRESS_PORT, ((unsigned long) 0x80000000 | (bus << 16) | (slot << 11) | (func << 8) | addr));
    outl(PCI_CONFIG_ADDRESS_PORT, value);
}

// Device class and subclass http://my.execpc.com/~geezer/code/pci.c

void pci_get_vendor(uint8_t bus, uint8_t slot) {
    /* vendors that == 0xFFFF, it must be a non-existent device. */
    // The first configuration register contains the vendor ID
    // And the vendor ID is reported as "all 1's" if there's no device connected
    for (int function = 0; function < 8; function++) {
        uint16_t vendor_id = pci_config_read_word(bus, slot, function, 0);
        if (vendor_id != PCI_VENDOR_NONE) {
            uint16_t device = pci_config_read_word(bus, slot, function, 2);
            const char* vendor = pci_vendor_name_for_id(vendor_id);
            uint16_t class_and_subclass = pci_config_read_word(bus, slot, function, 0x0F);
            printf("%s device found in bus %d slot %d: 0x%04x, class_and_sub\n", vendor, bus, slot, device);
            printf("\tDevice ID:            0x%04x\n", device);
            printf("\tFunction:             0x%04x\n", function);

            printf("\tClass and subclass:   0x%04x\n", class_and_subclass);
            uint32_t i = pci_read_dword(bus, slot, function, 0x08);
			uint8_t _class	 = (unsigned)((i >> 24) & 0xFF);
			uint8_t subclass = (unsigned)((i >> 16) & 0xFF);
            printf("\t2        0x%08x       0x%02x 0x%02x\n", i, _class, subclass);
            uint32_t j = pci_config_read_word(bus, slot, function, 0x0a);
            printf("\t2        0x%08x       0x%02x 0x%02x\n", j, ((j >> 8) & 0xff), (j) & 0xff);

            uint16_t bist_and_header_type = pci_config_read_word(bus, slot, function, 0x0E);
            printf("\tBIST and header type: 0x%04x %d\n", bist_and_header_type, (uint8_t)bist_and_header_type);
            uint8_t header_type = (uint8_t)bist_and_header_type;

            // Does the device support multiple functions?
            // https://forum.osdev.org/viewtopic.php?t=9987
            if (header_type & (1 << 8)) {
                // TODO(PT): Poll each function (8 possible? or more?)
            }
            else {
                // TODO(PT): Only one function available
            }

            // TODO(PT): This If doesn't work because it doesn't consider devices that support multiple functions
            if (header_type == PCI_DEVICE_HEADER_TYPE_DEVICE) {
                uint16_t interrupt_line_and_pin = pci_config_read_word(bus, slot, function, 0x3C);
                printf("\tIntl&pin              0x%04x\n", interrupt_line_and_pin);
                printf("\tInterrupt line:           %d\n", interrupt_line_and_pin & 0xff);
                printf("\tInterrupt pin:            %d %d\n", ((interrupt_line_and_pin & 0xff00) >> 4), (uint8_t)(interrupt_line_and_pin >> 8));
            }
            else {
                //NotImplemented();
                printf("Header type 0x%08x\n", header_type);
            }
        }
        else {
            //printf("No device in bus %d slot %d\n", bus, slot);
        }
    }
}

static void pci_init() {
    // TODO(PT): Do PCI init in a driver
    // https://forum.osdev.org/viewtopic.php?f=1&t=30546
    // https://gist.github.com/extremecoders-re/e8fd8a67a515fee0c873dcafc81d811c
    // https://www.qemu.org/2018/05/31/nic-parameter/
    for (int bus = 0; bus < 256; bus++) {
        for (int device_slot = 0; device_slot < 32; device_slot++) {
            // Is there a device plugged into this slot?
            uint16_t vendor_id = pci_config_read_word(bus, device_slot, 0, 0);
            if (vendor_id == PCI_VENDOR_NONE) {
                continue;
            }

            uint16_t tmp = pci_config_read_word(bus, device_slot, 0, 0x0e);
            uint8_t bist = (tmp >> 8) & 0xff;
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

                printf("%s %s\n", vendor_name, device_name);
                printf("\t%s %s\n", device_subclass_name, device_class_name);
                printf("\tBFD %d:%d:%d, ID %04x:%04x\n", bus, device_slot, function, vendor_id, device_id);

                device_id = pci_config_read_word(bus, device_slot, function, 2);
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
            }
        }
    }
}

void callback(registers_t* regs) {
    //printf("CALLBACK\n");
    uint16_t status = inw(0xc000 + 0x3e);

    #define TOK     (1<<2)
    #define ROK                 (1<<0)
    if(status & TOK) {
        printf("Packet sent\n");
    }
    if (status & ROK) {
        printf("Received packet\n");
        //receive_packet();
    }

    outw(0xc000 + 0x3E, 0x5);
}


#define CMD 0x37
#define IMR 0x3c // Interrupt Mask Register
#define ISR 0x3e // Interrupt Status Register
#define RCR 0x44 // Receive Config Register
#define CONFIG_1 0x52
#define RX_OK 0x01
#define TX_OK 0x04
#define TX_ERR 0x08
#define RCR_AAP  (1 << 0) /* Accept All Packets */
#define RCR_APM  (1 << 1) /* Accept Physical Match Packets */
#define RCR_AM   (1 << 2) /* Accept Multicast Packets */
#define RCR_AB   (1 << 3) /* Accept Broadcast Packets */
#define RCR_WRAP (1 << 7) /* Wrap packets too long */
#define RX_BUF 0x30
#define RX_BUF_PTR 0x38
#define RX_BUF_ADDR 0x3a
#define RX_MISSED 0x4c

char* rx_buffer[(1024*8)+16] = {0};
void realtek_8139_init(uint32_t bus, uint32_t device_slot, uint32_t function, uint32_t io_base) {
    // https://wiki.osdev.org/RTL8139
    printf("Init realtek_8139 at %d,%d,%d, IO Base 0x%04x\n", bus, device_slot, function, io_base);
    uint32_t command_register = pci_config_read_word(bus, device_slot, function, 0x04);
    printf("CmdReg before enable 0x%08x\n", command_register);
    // Enable bus mastering bit
    command_register |= (1 << 2);
    printf("CmdReg after enable 0x%08x\n", command_register);
    pci_write_dword(bus, device_slot, function, 0x04, command_register);

    // Power on the device
    #define REALTEK_8139_COMMAND_REGISTER_OFF 0x37
    #define REALTEK_8139_CONFIG0_REGISTER_OFF 0x51
    #define REALTEK_8139_CONFIG_1_REGISTER_OFF 0x52
    outb(io_base + REALTEK_8139_CONFIG_1_REGISTER_OFF, 0x0);
    outb(io_base + 0x50, 0x0);

    uint32_t tmp = inb(io_base + 0x52);
    printf("Config1 %d %d\n", (tmp >> 3) & 0x1, (tmp >> 2) & 0x1);

    // Software reset
    outb(io_base + REALTEK_8139_COMMAND_REGISTER_OFF, (1 << 4));
    while((inb(io_base + REALTEK_8139_COMMAND_REGISTER_OFF) & (1 << 4)) != 0) {
        printf("spin awaiting reset completion\n");
    }

    // Receiver enable
    outb(io_base + REALTEK_8139_COMMAND_REGISTER_OFF, (1 << 3));
    while((inb(io_base + REALTEK_8139_COMMAND_REGISTER_OFF) & (1 << 3)) == 0) {
        printf("spin awaiting rx enable completion\n");
    }

    // Transmitter enable
    outb(io_base + REALTEK_8139_COMMAND_REGISTER_OFF, (1 << 2));
    while((inb(io_base + REALTEK_8139_COMMAND_REGISTER_OFF) & (1 << 2)) == 0) {
        printf("spin awaiting tx enable completion\n");
    }

    printf("Buffer empty? %d\n", inb(io_base + REALTEK_8139_COMMAND_REGISTER_OFF) & (1 << 0));

    // Init receive buffer
    // Note: rx_buffer must be a pointer to a physical address
    // The OSDev Wiki recommends 8k + 16 bytes
    outl(io_base + 0x30, &rx_buffer);

    // Enable the "Transmit OK" and "Receive OK" interrupts
    //outw(io_base + 0x3C, 0x0005);
    //outw(io_base + IMR, RX_OK | TX_OK | TX_ERR);

    // Enable all interrupts
    outw(io_base + IMR, 0x3f | (1 << 13) | (1 << 14) | (1 << 15));
    tmp = inl(io_base + 0x44);
    printf("tmp 0x%08x\n", tmp);
    tmp &= ~0xe000;
    tmp &= ~0x1800;
    tmp |= (1 << 5);
    tmp |= (1 << 4);
    tmp |= (1 << 3);
    tmp |= (1 << 2);
    tmp |= (1 << 1);
    tmp |= (1 << 0);
    printf("rewrite 0x%08x\n", tmp);
    outl(io_base + 0x44, tmp);
    tmp = inl(io_base + 0x44);
    printf("tmp 0x%08x\n", tmp);

    /*
    uint16_t word = inw(io_base + 0x3e);
    while (1) {
        asm("sti");
        for (int i =0; i < 16; i++) {
            printf("%d",(word >> i) & 0x1);
        }
        printf("\n");
        asm("hlt");
    }
    */


    // 7. Set RCR (Receive Configuration Register)
    outb(io_base + RCR, RCR_AAP | RCR_APM | RCR_AM | RCR_AB | RCR_WRAP);

    // (1 << 7) is the WRAP bit, 0xf is AB+AM+APM+AAP
    //outl(io_base + 0x44, 0x0f | (1 << 7));

    uint32_t irq_number = pci_config_read_word(bus,
			device_slot, function, 0x3c) & 0xff;
    printf("irq_number %d\n", irq_number);

    interrupt_setup_callback(INT_VECTOR_IRQ11, callback);

    uint32_t mac_part1 = inl(io_base + 0x00);
    uint16_t mac_part2 = inw(io_base + 0x04);
    uint8_t mac_addr[8];
	mac_addr[0] = mac_part1 >> 0;
	mac_addr[1] = mac_part1 >> 8;
	mac_addr[2] = mac_part1 >> 16;
	mac_addr[3] = mac_part1 >> 24;
	mac_addr[4] = mac_part2 >> 0;
	mac_addr[5] = mac_part2 >> 8;
    printf("MAC address: %x:%x:%x:%x:%x:%x\n",
                mac_addr[0],
                mac_addr[1],
                mac_addr[2],
                mac_addr[3],
                mac_addr[4],
                mac_addr[5]);

    // Enable receiver and transmitter
    // Sets the RE and TE bits high
    outb(io_base + REALTEK_8139_COMMAND_REGISTER_OFF, 0x0C); 
    asm("sti");

    uint32_t* packet_addr = &rx_buffer[0];
    //00 18 8b 75 1d e0 00 1f f3 d8 47 ab 08 0
    /*
    rx_buffer[0] = 0x00;
    rx_buffer[1] = 0x18;
    rx_buffer[2] = 0x8b;
    rx_buffer[3] = 0x75;
    rx_buffer[4] = 0x1d;
    rx_buffer[5] = 0xe0;
    rx_buffer[6] = 0x00;
    rx_buffer[7] = 0x1f;
    rx_buffer[8] = 0xf3;
    rx_buffer[9] = 0xd8;
    rx_buffer[10] = 0x47;
    rx_buffer[11] = 0xab;
    rx_buffer[12] = 0x08;
    rx_buffer[13] = 0x00;
    */
   while (pit_clock() < 1000) {
       if (pit_clock() % 100 == 0) {
           printf("pic = %d\n", pit_clock());
       }
       asm("hlt");
   }
   printf("Will sent int now\n");
    // 192.168.1.5: MAC b8:27:eb:b2:ec:75
    rx_buffer[0] = 0xb8;
    rx_buffer[1] = 0x27;
    rx_buffer[2] = 0xeb;
    rx_buffer[3] = 0xb2;
    rx_buffer[4] = 0xec;
    rx_buffer[5] = 0x75;

    // We are MAC 52:54:00:12:34:56
    rx_buffer[6] = 0x52;
    rx_buffer[7] = 0x54;
    rx_buffer[8] = 0x00;
    rx_buffer[9] = 0x12;
    rx_buffer[10] = 0x34;
    rx_buffer[11] = 0x56;

    // Protocol type 0x806: ARP
    rx_buffer[12] = 0x08;
    rx_buffer[13] = 0x06;

    // Second, fill in physical address of data, and length
    outl(io_base + 0x20, packet_addr); 
    outl(io_base + 0x10, 14); 
}

uint32_t initial_esp = 0;
void kernel_main(struct multiboot_info* mboot_ptr, uint32_t initial_stack) {
    initial_esp = initial_stack;
    text_mode_init();

    // Environment info
    boot_info_read(mboot_ptr);
    boot_info_dump();

    // Descriptor tables
    gdt_init();
    interrupt_init();

    // PIT and serial drivers
    // Set this to 1ms to cause issues with keystrokes being lost
    // TODO(PT): Currently, a task-switch is a side-effect of a PIT ISR
    // In the future, work like scheduling should be done outside the ISR
    pit_timer_init(PIT_TICK_GRANULARITY_50MS);
    /*

    serial_init();

    // Kernel features
    pmm_init();
    pmm_dump();
    vmm_init();
    vmm_notify_shared_kernel_memory_allocated();
    syscall_init();
    initrd_init();

    // We've now allocated all the kernel memory that'll be mapped into every process 
    // Inform the VMM so that it can begin allocating memory outside of shared page tables
    vmm_dump(boot_info_get()->vmm_kernel);

    // Higher-level features like multitasking
    tasking_init();

    // Initialize PS/2 controller
    // (and sub-drivers, such as a PS/2 keyboard and mouse)
    ps2_controller_init();
    */

    // Early boot is finished
    // Multitasking and program loading is now available

    // Launch some initial drivers and services
    /*
    task_spawn(ps2_keyboard_driver_launch, PRIORITY_DRIVER, "");
    task_spawn(ps2_mouse_driver_launch, PRIORITY_DRIVER, "");
    task_spawn(awm_init, PRIORITY_GUI, "");
    task_spawn(tty_init, PRIORITY_TTY, "");
    task_spawn(rainbow, PRIORITY_NONE, "");
    task_spawn(paintbrush, 2, "");
    task_spawn(textpad, 3, "");
    */
    pci_init();
    asm("sti");
    while (1) {
        asm("hlt");
    }

    //task_spawn(cat);
    //task_spawn(rainbow);

    // Bootstrapping complete - kill this process
    printf("[t = %d] Bootstrap task [PID %d] will exit\n", time(), getpid());
    task_die(0);
    assert(0, "task_die should have stopped execution");
}
