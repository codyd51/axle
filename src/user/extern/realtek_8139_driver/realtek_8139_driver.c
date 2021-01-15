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

// TODO(PT): This MUST be a physical memory buffer!
char* rx_buffer[(1024*8)+16] = {0};
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

	/*
    uint32_t irq_number = pci_config_read_word(bus,
			device_slot, function, 0x3c) & 0xff;
    printf("irq_number %d\n", irq_number);
	*/

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
		asm("sti");
		// Wake on interrupt or AMC message
		// Problem: we now have a process that's using both `adi_interrupt_await` and `amc_message_await`
		// One to respond to an interrupt, the other to respond to a mouse event
		// Either we need threads or 2 procs
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
		printf("Awoke for interrupt? %d\n", awoke_for_interrupt);
		if (awoke_for_interrupt) {
			// The NIC needs some attention
			printf("CALLBACK\n");
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
			else {
				printf("Unknown status 0x%04x\n", status);
			}

			outw(0xc000 + 0x3E, 0x5);
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
						uint32_t* packet_addr = rx_buffer;
						//00 18 8b 75 1d e0 00 1f f3 d8 47 ab 08 0
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
						uint8_t tx_buffer_port = tx_buffer_register_ports[tx_round_robin_counter];
						uint8_t tx_status_command_port = tx_status_command_register_ports[tx_round_robin_counter];
						tx_round_robin_counter += 1;
						if (tx_round_robin_counter > sizeof(tx_buffer_register_ports) / sizeof(tx_buffer_register_ports[0])) {
							tx_round_robin_counter = 0;
						}
						outl(io_base + tx_buffer_port, packet_addr); 
						outl(io_base + tx_status_command_port, 14); 
					}
				}
			} while (amc_has_message_from("com.axle.awm"));
		}
	}
	
	return 0;
}
