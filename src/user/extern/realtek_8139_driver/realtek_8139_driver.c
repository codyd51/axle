#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
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

// Port IO
#include <libport/libport.h>

// Network stack communication
#include <net/net_messages.h>

#include "realtek_8139_driver.h"
#include "rtl8139_messages.h"

// Many graphics lib functions call gfx_screen() 
Screen _screen = {0};
Screen* gfx_screen() {
	return &_screen;
}

static ca_layer* window_layer_get(uint32_t width, uint32_t height) {
	// Ask awm to make a window for us
	amc_msg_u32_3__send("com.axle.awm", AWM_REQUEST_WINDOW_FRAMEBUFFER, width, height);

	// And get back info about the window it made
	amc_message_t* receive_framebuf;
	amc_message_await("com.axle.awm", &receive_framebuf);
	uint32_t event = amc_msg_u32_get_word(receive_framebuf, 0);
	if (event != AWM_CREATED_WINDOW_FRAMEBUFFER) {
		printf("Invalid state. Expected framebuffer command\n");
	}
	uint32_t framebuffer_addr = amc_msg_u32_get_word(receive_framebuf, 1);

	printf("Received framebuffer from awm: %d 0x%08x\n", event, framebuffer_addr);
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
#define REALTEK_8139_COMMAND_REGISTER_OFF 0x37
#define REALTEK_8139_CONFIG0_REGISTER_OFF 0x51
#define REALTEK_8139_CONFIG_1_REGISTER_OFF 0x52

typedef struct rtl8139_state {
	// Configuration encoded into the device
	uint8_t tx_buffer_register_ports[4];
	uint8_t tx_status_command_register_ports[4];

	// Configuration read from PCI bus
	uint32_t pci_bus;
	uint32_t pci_device_slot;
	uint32_t pci_function;
	uint32_t io_base;

	// Configuration assigned by this driver
	uint32_t receive_buffer_virt;
	uint32_t receive_buffer_phys;
	uint32_t transmit_buffer_virt;
	uint32_t transmit_buffer_phys;

	// Current running state
	uint8_t tx_round_robin_counter;
	uint32_t rx_curr_buf_off;
} rtl8139_state_t;

static void receive_packet(rtl8139_state_t* state) {
    while ((inb(state->io_base + RTL_REG_COMMAND_REGISTER) & RTL_CMD_REG_IS_RX_BUFFER_EMPTY) == 0) {
		uint16_t* packet = (uint16_t*)(state->receive_buffer_virt + state->rx_curr_buf_off);
		// The RTL8139 stores the packet metadata in the first 4 bytes of the buffer
		uint16_t packet_header = packet[0];
		uint16_t packet_len = packet[1];

		/*
		printf("Packet info:\n");
		printf("\tValid:   %s\n", (packet_header & RTL_RX_PACKET_STATUS_ROK) ? "yes" : "no");
		printf("\tLength:  %d\n", packet_len);
		printf("\tMAC address match? %s\n", (packet_header & RTL_RX_PACKET_STATUS_MAC_ADDRESS_MATCHES) ? "yes" : "no");

		if (packet_header & RTL_RX_PACKET_STATUS_IS_BROADCAST) {
			printf("\tBroadcast packet\n");
		}
		if (packet_header & RTL_RX_PACKET_STATUS_IS_MULTICAST) {
			printf("\tMulticast packet\n");
		}

		if (packet_header & RTL_RX_PACKET_STATUS_LONG) {
			printf("\tLong packet\n");
		}
		if (packet_header & RTL_RX_PACKET_STATUS_RUNT) {
			printf("\tRunt packet\n");
		}

		if (packet_header & RTL_RX_PACKET_STATUS_FRAME_ALIGNMENT_ERROR) {
			printf("\tFrame alignment error!\n");
		}
		if (packet_header & RTL_RX_PACKET_STATUS_CRC_ERROR) {
			printf("\tCRC error!\n");
		}
		if (packet_header & RTL_RX_PACKET_STATUS_INVALID_SYMBOL_ERROR) {
			printf("\tInvalid symbol error!\n");
		}
		*/

		// Get the main packet buffer, after the metadata
		char* packet_data = (char*)&packet[2];
		// Copy into a safe buffer, and trim off the 4-byte CRC at the end
		uint32_t ethernet_frame_size = packet_len - 4;
		uint32_t packet_size = sizeof(net_packet_t) + ethernet_frame_size;
		net_packet_t* packet_msg = malloc(packet_size);
		packet_msg->common.event = NET_RX_ETHERNET_FRAME;
		packet_msg->len = ethernet_frame_size;
		memcpy(packet_msg->data, packet_data, ethernet_frame_size);
		amc_message_construct_and_send(NET_SERVICE_NAME, packet_msg, packet_size);
		free(packet_msg);

		/*
		ethernet_frame_t* ethernet_frame = malloc(ethernet_frame_size);
		memcpy(ethernet_frame, packet_data, ethernet_frame_size);
		_forward_packet(ethernet_frame, ethernet_frame_size);
		*/
		//hexdump(packet_data, min(packet_len, 256));

		// https://www.cs.usfca.edu/~cruse/cs326f04/RTL8139_ProgrammersGuide.pdf
		// packet_len already includes the 4-byte CRC at the end
		/*
		state->rx_curr_buf_off += packet_len;
		// And skip the 4-byte header at the beginning
		state->rx_curr_buf_off += sizeof(uint16_t) * 2;
		// And skip to the next 32-bit boundary
		uint8_t w32_mask = sizeof(uint32_t) - 1;
		state->rx_curr_buf_off = (state->rx_curr_buf_off + w32_mask) & ~w32_mask;
		*/
		state->rx_curr_buf_off = (state->rx_curr_buf_off + packet_len + 4 + 3) & ~3;

		if (state->rx_curr_buf_off > 8192) {
			//printf("rollback\n");
			state->rx_curr_buf_off -= 8192;
		}

		// I don't know why 0x10 is subtracted here, but other drivers do it and it works.
		outw(state->io_base + RTL_REG_RX_CURRENT_ADDR_PACKET_READ, state->rx_curr_buf_off - 0x10);
	}
}

static void _enable_device_bus_mastering(rtl8139_state_t* nic_state) {
	// Read the current PCI config word
	amc_message_t* response;
	amc_msg_u32_4__request_response_sync(
		&response,
		PCI_SERVICE_NAME,
		PCI_REQUEST_READ_CONFIG_WORD,
		PCI_RESPONSE_READ_CONFIG_WORD,
		nic_state->pci_bus,
		nic_state->pci_device_slot,
		nic_state->pci_function,
		0x04
	);
	uint32_t command_register = amc_msg_u32_get_word(response, 1);
	printf("Command register 0x%08x\n", command_register);

	// Enable IO port access (though it should already be set)
	command_register |= (1 << 0);
	// Enable MMIO access (though it should already be set)
	command_register |= (1 << 1);
	// Enable bus mastering
	command_register |= (1 << 2);

	// Write the modified PCI config word
	amc_msg_u32_5__request_response_sync(
		&response,
		PCI_SERVICE_NAME,
		PCI_RESPONSE_WRITE_CONFIG_WORD,
		PCI_RESPONSE_WRITE_CONFIG_WORD,
		nic_state->pci_bus,
		nic_state->pci_device_slot,
		nic_state->pci_function,
		0x04,
		command_register
	);
	printf("Sent command to set command register to 0x%08x\n", command_register);
}

static void _perform_command(rtl8139_state_t* nic, uint8_t command, uint8_t expect) {
    outb(nic->io_base + RTL_REG_COMMAND_REGISTER, command);
	/*
    while((inb(nic->io_base + RTL_REG_COMMAND_REGISTER) & command) != expect) {
		printf("Spin awaiting RLT8139 command completion\n");
    }
	*/
}

void realtek_8139_init(uint32_t bus, uint32_t device_slot, uint32_t function, uint32_t io_base, rtl8139_state_t* out_state) {
    // https://wiki.osdev.org/RTL8139
    printf("Init realtek_8139 at %d,%d,%d, IO Base 0x%04x\n", bus, device_slot, function, io_base);

	// Init default state and set up our input parameters 
	memset(out_state, 0, sizeof(rtl8139_state_t));
	out_state->pci_bus = bus;
	out_state->pci_device_slot = device_slot;
	out_state->pci_function = function;
	out_state->io_base = io_base;
	out_state->tx_round_robin_counter = 0;
	for (int i = 0; i < 4; i++) {
		out_state->tx_buffer_register_ports[i] = 0x20 + (i * sizeof(uint32_t));
		out_state->tx_status_command_register_ports[i] = 0x10 + (i * sizeof(uint32_t));
	}

    // Power on the device
    outb(io_base + REALTEK_8139_CONFIG_1_REGISTER_OFF, 0x0);

	// Enable bus mastering (must be done after power on)
	_enable_device_bus_mastering(out_state);

    // Software reset
	_perform_command(out_state, RTL_CMD_REG_RESET, 0);

	// According to http://www.jbox.dk/sanos/source/sys/dev/rtl8139.c.html,
	// we must enable Tx/Rx before setting transfer thresholds
	// Enable the receiver and transmitter
	// It seems they must be sent in the same command 
	_perform_command(out_state, RTL_CMD_REG_ENABLE_RECEIVE | RTL_CMD_REG_ENABLE_TRANSMIT, 1);

	// Set up RX and TX buffers, and give them to the device
	uint32_t virt_memory_rx_addr = 0;
	uint32_t phys_memory_rx_addr = 0;
	uint32_t rx_buffer_size = 8192 + 16 + 1500;
	amc_physical_memory_region_create(rx_buffer_size, &virt_memory_rx_addr, &phys_memory_rx_addr);
	out_state->receive_buffer_virt = virt_memory_rx_addr;
	out_state->receive_buffer_phys = phys_memory_rx_addr;
	printf("Set RX buffer phys: 0x%08x\n", phys_memory_rx_addr);
	outl(io_base + RTL_REG_RX_BUFFER_PHYS_START, phys_memory_rx_addr);

	uint32_t virt_memory_tx_addr = 0;
	uint32_t phys_memory_tx_addr = 0;
	amc_physical_memory_region_create((1024*8) + 16, &virt_memory_tx_addr, &phys_memory_tx_addr);
	out_state->transmit_buffer_virt = virt_memory_tx_addr;
	out_state->transmit_buffer_phys = phys_memory_tx_addr;
	printf("Set TX buffer phys: 0x%08x\n", phys_memory_tx_addr);
	outl(io_base + RTL_REG_TX_0_PHYS_START, phys_memory_tx_addr);
	outl(io_base + RTL_REG_TX_1_PHYS_START, phys_memory_tx_addr);
	outl(io_base + RTL_REG_TX_2_PHYS_START, phys_memory_tx_addr);
	outl(io_base + RTL_REG_TX_3_PHYS_START, phys_memory_tx_addr);
	// TODO(PT): Do we need to set up TSAD1,2,3?

	// Enable all interrupts
	uint32_t int_mask = 0;
	int_mask |= RTL_ISR_FLAG_ROK;
	//int_mask |= RTL_ISR_FLAG_RER;
	int_mask |= RTL_ISR_FLAG_TOK;
	int_mask |= RTL_ISR_FLAG_TER;
	int_mask |= RTL_ISR_FLAG_RX_BUFFER_OVERFLOW;
	int_mask |= RTL_ISR_FLAG_LINK_CHANGE;
	int_mask |= RTL_ISR_FLAG_RX_FIFO_OVERFLOW;
    outw(io_base + RTL_REG_INTERRUPT_MASK, int_mask);

	// Configure the types of packets that should be received
	// And disable "wrap packet" mode
	uint32_t receive_config = 0;
	receive_config |= RTL_RX_CONFIG_FLAG_ACCEPT_ALL_PACKETS;
	receive_config |= RTL_RX_CONFIG_FLAG_ACCEPT_MAC_MATCH_PACKETS;
	receive_config |= RTL_RX_CONFIG_FLAG_ACCEPT_MULTICAST_PACKETS;
	receive_config |= RTL_RX_CONFIG_FLAG_ACCEPT_BROADCAST_PACKETS;
	receive_config |= RTL_RX_CONFIG_FLAG_ACCEPT_RUNT_PACKETS;
	receive_config |= RTL_RX_CONFIG_FLAG_ACCEPT_ERROR_PACKETS;
	receive_config |= RTL_RX_CONFIG_FLAG_ACCEPT_RUNT_PACKETS;
	receive_config |= RTL_RX_CONFIG_FLAG_DO_NOT_WRAP;
	outl(io_base + RTL_REG_RX_CONFIG, receive_config);

	/*
	// Set max DMA burst to "unlimited"
	receive_config |= (7 << 8);
	// Set RX buffer length to 8k + 16
	receive_config &= ~(00 << 11);
	*/
}

static void send_packet(rtl8139_state_t* nic_state, uint8_t* packet_data, uint32_t packet_len) {
	uint8_t* buffer = (uint8_t*)nic_state->transmit_buffer_virt;
	memcpy(buffer, packet_data, packet_len);

	// Second, fill in physical address of data, and length
	uint8_t tx_buffer_port = nic_state->tx_buffer_register_ports[nic_state->tx_round_robin_counter];
	uint8_t tx_status_command_port = nic_state->tx_status_command_register_ports[nic_state->tx_round_robin_counter];
	nic_state->tx_round_robin_counter += 1;
	if (nic_state->tx_round_robin_counter >= sizeof(nic_state->tx_buffer_register_ports) / sizeof(nic_state->tx_buffer_register_ports[0])) {
		nic_state->tx_round_robin_counter = 0;
	}
	// TODO(PT): I don't know what the consequences are for using one buffer for all
	// TSD's. Perhaps we actually need 4 buffers
	outl(nic_state->io_base + tx_buffer_port, nic_state->transmit_buffer_phys); 

	uint32_t transmit_status = (packet_len);
	// Set bit 13 (OWN bit) to zero to start the transfer
	transmit_status &= ~(1 << 13);
	outl(nic_state->io_base + tx_status_command_port, transmit_status); 
	// https://forum.osdev.org/viewtopic.php?f=1&t=26938
}

static void _read_mac_address(rtl8139_state_t* nic_state, char* buf, uint32_t bufsize) {
    uint8_t mac_addr[6];
	uint32_t mac_part1 = inl(nic_state->io_base + RTL_REG_ID_0);
	mac_addr[0] = (uint8_t)(mac_part1 >> 0);
	mac_addr[1] = (uint8_t)(mac_part1 >> 8);
	mac_addr[2] = (uint8_t)(mac_part1 >> 16);
	mac_addr[3] = (uint8_t)(mac_part1 >> 24);
    uint16_t mac_part2 = inw(nic_state->io_base + RTL_REG_ID_4);
	mac_addr[4] = (uint8_t)(mac_part2 >> 0);
	mac_addr[5] = (uint8_t)(mac_part2 >> 8);
	snprintf(buf, bufsize, "%02x:%02x:%02x:%02x:%02x:%02x\n", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
}

int main(int argc, char** argv) {
	// This process will handle interrupts from the Realtek 8159 NIC (IRQ11)
	// TODO(PT): The interrupt number is read from the PCI bus
	// It should be communicated to this process
	adi_register_driver(RTL8139_SERVICE_NAME, INT_VECTOR_IRQ11);
	amc_register_service(RTL8139_SERVICE_NAME);

	// TODO(PT): This should be read from the PCI bus
	int io_base = 0xc000;
	rtl8139_state_t nic_state = {0};
	realtek_8139_init(0, 3, 0, io_base, &nic_state);

	Size window_size = size_make(300, 70);
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

	text_box_puts(text_box, "MAC address: ", color_white());
	char mac_buf[256];
	_read_mac_address(&nic_state, mac_buf, sizeof(mac_buf));
	text_box_puts(text_box, mac_buf, color_purple());

    // Blit the text box to the window layer
    blit_layer(window_layer, text_box->layer, text_box_frame, rect_make(point_zero(), text_box_frame.size));
    // And ask awm to draw our window
	amc_msg_u32_1__send("com.axle.awm", AWM_WINDOW_REDRAW_READY);

	while (true) {
		// Wake on interrupt or AMC message
		bool awoke_for_interrupt = adi_event_await(INT_VECTOR_IRQ11);
		if (awoke_for_interrupt) {
			// The NIC needs some attention
			// Reading the ISR register clears all interrupts
			uint16_t status = inw(nic_state.io_base + RTL_REG_INTERRUPT_STATUS);
			if (status & RTL_ISR_FLAG_TOK) {
				printf("Packet sent\n");
			}
			if (status & RTL_ISR_FLAG_ROK) {
				receive_packet(&nic_state);
			}
			if (status & RTL_ISR_FLAG_TER) {
				printf("Transmit error\n");
			}
			if (status & RTL_ISR_FLAG_RER) {
				printf("Receive error\n");
			}

			// This is not mutually exclusive with the other flags
			if (status & RTL_ISR_FLAG_RX_BUFFER_OVERFLOW) {
				printf("RX buffer is full!\n");
			}

			// Set the OWN bit to zero
			//status &= ~(1 << 13);
			//status &= ~(1 << 2);
			outw(nic_state.io_base + RTL_REG_INTERRUPT_STATUS, status);
			adi_send_eoi(INT_VECTOR_IRQ11);
		}
		else {
			// We woke to service amc
			amc_message_t* msg;
			do {
				amc_message_await_any(&msg);
				const char* source_service = msg->source;
				if (!strcmp(source_service, NET_SERVICE_NAME)) {
					net_message_t* net_msg = (net_message_t*)msg->body;
					uint8_t event = net_msg->packet.common.event;
					if (event == NET_TX_ETHERNET_FRAME) {
						uint8_t* packet_data = &net_msg->packet.data;
						printf("tx ethernet frame len = %d data 0x%08x!\n", net_msg->packet.len, packet_data);
						send_packet(&nic_state, packet_data, net_msg->packet.len);
					}
					else if (event == NET_REQUEST_NIC_CONFIG) {
						printf("Net requested NIC config...\n");
						net_nic_config_info_t config_msg;
						config_msg.common.event = NET_RESPONSE_NIC_CONFIG;

						uint32_t mac_part1 = inl(nic_state.io_base + 0x00);
						uint16_t mac_part2 = inw(nic_state.io_base + 0x04);
						uint8_t mac_addr[8];
						mac_addr[0] = mac_part1 >> 0;
						mac_addr[1] = mac_part1 >> 8;
						mac_addr[2] = mac_part1 >> 16;
						mac_addr[3] = mac_part1 >> 24;
						mac_addr[4] = mac_part2 >> 0;
						mac_addr[5] = mac_part2 >> 8;
						memcpy(&config_msg.mac_addr, mac_addr, 6);

						amc_message_construct_and_send(NET_SERVICE_NAME, &config_msg, sizeof(net_nic_config_info_t));
					}
					else {
						printf("Unknown event from net service: %d\n", event);
					}
				}
				else if (!strcmp(source_service, "com.axle.awm")) {
					// Ignore awm messages
				}
				else {
					printf("RTL8139 driver got command from unknown service %s\n", source_service);
				}

			} while (amc_has_message());
		}
	}
	
	return 0;
}
