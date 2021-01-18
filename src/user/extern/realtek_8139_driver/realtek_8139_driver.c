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

// PCI
#include <pci/pci_messages.h>

// Communication with other processes
#include <libamc/libamc.h>

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

void realtek_8139_init(uint32_t bus, uint32_t device_slot, uint32_t function, uint32_t io_base) {
    // https://wiki.osdev.org/RTL8139
    printf("Init realtek_8139 at %d,%d,%d, IO Base 0x%04x\n", bus, device_slot, function, io_base);
	// TODO(PT): Enable bus mastering bit as a message to the PCI driver
	// Currently it's done before this driver runs

    // Power on the device
    #define REALTEK_8139_COMMAND_REGISTER_OFF 0x37
    #define REALTEK_8139_CONFIG0_REGISTER_OFF 0x51
    #define REALTEK_8139_CONFIG_1_REGISTER_OFF 0x52
    outb(io_base + REALTEK_8139_CONFIG_1_REGISTER_OFF, 0x0);
    //outb(io_base + 0x50, 0x0);

	// Enable bus mastering (must be done after power on)
	amc_msg_u32_5__send(PCI_SERVICE_NAME, PCI_REQUEST_READ_CONFIG_WORD, bus, device_slot, function, 0x04);
	// TODO(PT): Wrap up into sync interface
	// TODO(PT): Should loop until the message is the desired one, discarding others
	amc_command_message_t recv = {0};
	amc_message_await(PCI_SERVICE_NAME, &recv);
	// TODO(PT): Need a struct type selector
	if (amc_msg_u32_get_word(&recv, 0) != PCI_RESPONSE_READ_CONFIG_WORD) {
		printf("Invalid state. Expected response for read config word\n");
	}
	uint32_t command_register = amc_msg_u32_get_word(&recv, 1);

	// Enable IO port access (though it should already be set)
	command_register |= (1 << 0);
	// Enable MMIO access (though it should already be set)
	command_register |= (1 << 1);
	// Enable bus mastering
	command_register |= (1 << 2);

	amc_msg_u32_6__send(PCI_SERVICE_NAME, PCI_REQUEST_WRITE_CONFIG_WORD, bus, device_slot, function, 0x04, command_register);
	printf("Sent command to set command register to 0x%08x\n", command_register);
	amc_message_await(PCI_SERVICE_NAME, &recv);
	if (amc_msg_u32_get_word(&recv, 0) != PCI_RESPONSE_WRITE_CONFIG_WORD) {
		printf("Invalid state. Expected response for write config word\n");
	}

    // Software reset
    outb(io_base + REALTEK_8139_COMMAND_REGISTER_OFF, (1 << 4));
    while((inb(io_base + REALTEK_8139_COMMAND_REGISTER_OFF) & (1 << 4)) != 0) {
        printf("spin awaiting reset completion\n");
    }

	uint32_t virt_memory_rx_addr = 0;
	uint32_t phys_memory_rx_addr = 0;
	uint32_t rx_buffer_size = 8192 + 16 + 1500;
	amc_physical_memory_region_create(rx_buffer_size, &virt_memory_rx_addr, &phys_memory_rx_addr);
	printf("Set RX buffer phys: 0x%08x\n", phys_memory_rx_addr);
	outl(io_base + 0x30, phys_memory_rx_addr);

    // Enable the "Transmit OK" and "Receive OK" interrupts
    //outw(io_base + 0x3C, 0x0005);
    //outw(io_base + IMR, RX_OK | TX_OK | TX_ERR);
	outw(io_base + 0x3C, 0x0005);

	outl(io_base + 0x44, 0xf | (1 << 7));

	// According to http://www.jbox.dk/sanos/source/sys/dev/rtl8139.c.html,
	// we must enable Tx/Rx before setting transfer thresholds
    // Receiver enable
	/*
    outb(io_base + REALTEK_8139_COMMAND_REGISTER_OFF, (1 << 3));
    while((inb(io_base + REALTEK_8139_COMMAND_REGISTER_OFF) & (1 << 3)) == 0) {
        printf("spin awaiting rx enable completion\n");
    }

    // Transmitter enable
    outb(io_base + REALTEK_8139_COMMAND_REGISTER_OFF, (1 << 2));
    while((inb(io_base + REALTEK_8139_COMMAND_REGISTER_OFF) & (1 << 2)) == 0) {
        printf("spin awaiting tx enable completion\n");
    }
	*/
	outb(io_base + 0x37, 0x0C);

	/*
	#define RCR_AcceptRunt (1 << 4)
	#define RECEIVE_CONFIG_ACCEPT_ERROR (1 << 5)
	#define RECEIVE_CONFIG_WRAP (1 << 7)
	uint32_t receive_config = RCR_AAP | RCR_APM | RCR_AM | RCR_AB | RCR_AcceptRunt | RECEIVE_CONFIG_ACCEPT_ERROR | RECEIVE_CONFIG_WRAP;
	// Set max DMA burst to "unlimited"
	receive_config |= (7 << 8);
	// Set RX buffer length to 8k + 16
	receive_config &= ~(00 << 11);
	printf("receive_config 0x%08x\n", receive_config);
	outl(io_base + RCR, receive_config);
	*/

    printf("Buffer empty? %d\n", inb(io_base + REALTEK_8139_COMMAND_REGISTER_OFF) & (1 << 0));

    // Init receive buffer
    // Note: rx_buffer must be a pointer to a physical address
    // The OSDev Wiki recommends 8k + 16 bytes
    //outl(io_base + 0x30, &rx_buffer);


    // Enable all interrupts
	/*
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
	*/

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

    // Enable receiver and transmitter
    // Sets the RE and TE bits high
    //outb(io_base + REALTEK_8139_COMMAND_REGISTER_OFF, 0x0C); 

    // 7. Set RCR (Receive Configuration Register)
    //outb(io_base + RCR, RCR_AAP | RCR_APM | RCR_AM | RCR_AB | RCR_WRAP);
	// (1 << 7) is the WRAP bit, 0xf is AB+AM+APM+AAP
    //outl(io_base + 0x44, 0x0F);

    // (1 << 7) is the WRAP bit, 0xf is AB+AM+APM+AAP
    //outl(io_base + 0x44, 0x0f | (1 << 7));

	/*
    uint32_t irq_number = pci_config_read_word(bus,
			device_slot, function, 0x3c) & 0xff;
    printf("irq_number %d\n", irq_number);
	*/

	/*
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
				*/
			/*
	uint32_t command_register = pci_config_read_word(dev->bus, dev->device_slot, dev->function, 0x04);
	printf("CmdReg before enable 0x%08x\n", command_register);
	// Enable bus mastering bit
	command_register |= (1 << 2);
	command_register &= ~0x400;
	printf("CmdReg after enable 0x%08x\n", command_register);
	pci_config_write_word(dev->bus, dev->device_slot, dev->function, 0x04, command_register);
	*/
	// https://stackoverflow.com/questions/87442/virtual-network-interface-in-mac-os-x

    asm("sti");
}

const uint8_t tx_buffer_register_ports[] = {0x20, 0x24, 0x28, 0x2C};
const uint8_t tx_status_command_register_ports[] = {0x10, 0x14, 0x18, 0x1C};
static uint8_t tx_round_robin_counter = 0;

int main(int argc, char** argv) {
	// This process will handle interrupts from the Realtek 8159 NIC (IRQ11)
	// TODO(PT): The interrupt number is read from the PCI bus
	// It should be communicated to this process
	adi_register_driver("com.axle.realtek_8139_driver", INT_VECTOR_IRQ11);
	amc_register_service("com.axle.realtek_8139_driver");

	int io_base = 0xc000;
	realtek_8139_init(0, 3, 0, io_base);

	Size window_size = size_make(500, 300);
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
	draw_rect(window_layer, window_frame, color_purple(), THICKNESS_FILLED);
	text_box_t* text_box = text_box_create(text_box_frame.size, color_black());

	text_box_puts(text_box, "RealTek 8139 NIC driver\n", color_white());

	uint32_t mac_part1 = inl(io_base + 0x00);
    uint16_t mac_part2 = inw(io_base + 0x04);
    uint8_t mac_addr[8];
	mac_addr[0] = mac_part1 >> 0;
	mac_addr[1] = mac_part1 >> 8;
	mac_addr[2] = mac_part1 >> 16;
	mac_addr[3] = mac_part1 >> 24;
	mac_addr[4] = mac_part2 >> 0;
	mac_addr[5] = mac_part2 >> 8;

	const char buf[256];
	snprintf(buf, sizeof(buf), "MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5], mac_addr[6]);

	text_box_puts(text_box, buf, color_white());
	text_box_puts(text_box, "Click here to send a packet!", color_white());

    // Blit the text box to the window layer
    blit_layer(window_layer, text_box->layer, text_box_frame, rect_make(point_zero(), text_box_frame.size));
    // And ask awm to draw our window
    amc_command_msg__send("com.axle.awm", AWM_WINDOW_REDRAW_READY);

	while (true) {
		// Wake on interrupt or AMC message
		// Even with 2 procs the issue is there: the driver proc would still need to listen for messages from the frontend
		// We really do need a proc to be able to wait for both ADI or AMC, but think about it more
		// TODO(PT): I might not have needed to remove the EOI from the adi_interrupt_await path -- 
		// the reason interrupts weren't coming through might have been because I didn't switch to the next TX buffer in the card. NBD
		// TODO(PT): A lot of the confusion here came from trying to to have this be both an amc and adi service
		// The issue being that an interrupt could be raised within the same process that handles the interrupt
		// Perhaps try this proc as an amc service that does respond to messages to construct packets, etc, but does not 
		// Perhaps if the driver proc is running, increment a "pending int count" if an int comes through.
		// adi_interrupt_await will decrement this count instead of blocking until it hits zero
		bool awoke_for_interrupt = adi_event_await(INT_VECTOR_IRQ11);
		//printf("ADI returned: %d\n", awoke_for_interrupt);
		if (awoke_for_interrupt) {
			// The NIC needs some attention
			// Reading the ISR register clears all interrupts
			uint16_t status = inw(0xc000 + ISR);
			printf("NIC interrupt. Status %04x\n", status);

			#define TOK     (1<<2)
			#define ROK                 (1<<0)
			if(status & TOK) {
				printf("Packet sent\n");
			}
			else if (status & ROK) {
				printf("Received packet\n");
				//receive_packet();
			}
			else {
				printf("Unknown status 0x%04x\n", status);
			}

			// Set the OWN bit to zero
			status &= ~(1 << 13);
			//status &= ~(1 << 2);
			outw(0xc000 + ISR, status);
			adi_send_eoi(INT_VECTOR_IRQ11);
		}
		else {
			// We woke to service amc
			amc_charlist_message_t msg = {0};
			do {
				amc_message_await("com.axle.awm", &msg);
				// TODO(PT): Need a struct type selector
				uint32_t event = amc_command_ptr_msg__get_command(&msg);
				if (event == AWM_KEY_DOWN) {
					char ch = amc_command_ptr_msg__get_ptr(&msg);
					if (ch == 'z') {
						printf("Sending packet!\n");

						uint32_t virt_memory_addr = 0;
						uint32_t phys_memory_addr = 0;
						amc_physical_memory_region_create((1024*8) + 16, &virt_memory_addr, &phys_memory_addr);
						outl(0xc000 + 0x20, phys_memory_addr);
						printf("Phys mem 0x%08x\n", phys_memory_addr);
						printf("Virt mem 0x%08x\n", virt_memory_addr);

						uint8_t* buffer = (uint32_t*)virt_memory_addr;
						memset(buffer, 'A', (1024*8)+16);
						//00 18 8b 75 1d e0 00 1f f3 d8 47 ab 08 0
						// 192.168.1.5: MAC b8:27:eb:b2:ec:75
						/*
						buffer[0] = 0xb8;
						buffer[1] = 0x27;
						buffer[2] = 0xeb;
						buffer[3] = 0xb2;
						buffer[4] = 0xec;
						buffer[5] = 0x75;

						// We are MAC 52:54:00:12:34:56
						buffer[6] = 0x52;
						buffer[7] = 0x54;
						buffer[8] = 0x00;
						buffer[9] = 0x12;
						buffer[10] = 0x34;
						buffer[11] = 0x56;

						// Protocol type 0x806: ARP
						buffer[12] = 0x08;
						buffer[13] = 0x06;
						*/

						// Second, fill in physical address of data, and length
						uint8_t tx_buffer_port = tx_buffer_register_ports[tx_round_robin_counter];
						uint8_t tx_status_command_port = tx_status_command_register_ports[tx_round_robin_counter];
						tx_round_robin_counter += 1;
						if (tx_round_robin_counter > sizeof(tx_buffer_register_ports) / sizeof(tx_buffer_register_ports[0])) {
							tx_round_robin_counter = 0;
						}
						outl(io_base + tx_buffer_port, phys_memory_addr); 
						outl(io_base + tx_status_command_port, (200)); 
						// https://forum.osdev.org/viewtopic.php?f=1&t=26938
						// TODO(PT): Give the tap device an IPv4 address on the host so we can ping it
					}
				}
			} while (amc_has_message_from("com.axle.awm"));
		}
	}
	
	return 0;
}
