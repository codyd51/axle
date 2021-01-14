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

static void pci_driver() {
    const char* program_name = "pci_driver";
    FILE* fp = initrd_fopen(program_name, "rb");
    char* argv[] = {program_name, NULL};
    elf_load_file(program_name, fp, argv);
    panic("noreturn");
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
    /*
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
    *


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

    uint32_t* packet_addr = rx_buffer;
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
    *
   while (pit_clock() < 500) {
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
    */
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

    // Early boot is finished
    // Multitasking and program loading is now available

    // Launch some initial drivers and services
    task_spawn(ps2_keyboard_driver_launch, PRIORITY_DRIVER, "");
    task_spawn(ps2_mouse_driver_launch, PRIORITY_DRIVER, "");
    task_spawn(awm_init, PRIORITY_GUI, "");
    task_spawn(tty_init, PRIORITY_TTY, "");
    //task_spawn(rainbow, PRIORITY_NONE, "");
    //task_spawn(paintbrush, 2, "");
    //task_spawn(textpad, 3, "");
    task_spawn(pci_driver, 4, "");
    //pci_init();
    /*
    asm("sti");
    while (1) {
        asm("hlt");
    }
    */

    //task_spawn(cat);
    //task_spawn(rainbow);

    // Bootstrapping complete - kill this process
    printf("[t = %d] Bootstrap task [PID %d] will exit\n", time(), getpid());
    task_die(0);
    assert(0, "task_die should have stopped execution");
}
