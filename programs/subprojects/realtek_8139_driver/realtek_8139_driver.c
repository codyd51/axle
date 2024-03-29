#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <kernel/adi.h>
#include <kernel/amc.h>
#include <kernel/idt.h>
#include <kernel/core_commands.h>

#include <libgui/libgui.h>

// PCI
#include <pci/pci_messages.h>

// IPC communication
#include <libamc/libamc.h>

// Port IO
#include <libport/libport.h>

// Network stack communication
#include <net/net_messages.h>

#include "realtek_8139_driver.h"
#include "rtl8139_messages.h"

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
	uintptr_t receive_buffer_virt;
	uintptr_t receive_buffer_phys;
	uintptr_t transmit_buffer_virt;
	uintptr_t transmit_buffer_phys;

	// Current running state
	uint8_t tx_round_robin_counter;
	uintptr_t rx_curr_buf_off;
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
		if (ethernet_frame_size >= 4096) {
			printf("*** NIC dropping nonsensically large packet: %d\n", ethernet_frame_size);
			printf("NIC head 0x%08x + %d = 0x%08x\n", state->rx_curr_buf_off, ethernet_frame_size, state->rx_curr_buf_off + ethernet_frame_size);
		}
		else {
			uint32_t amc_message_size = sizeof(net_message_t) + ethernet_frame_size;
			net_message_t* packet_msg = malloc(amc_message_size);
			packet_msg->event = NET_RX_ETHERNET_FRAME;
			packet_msg->m.packet.len = ethernet_frame_size;
			memcpy(packet_msg->m.packet.data, packet_data, ethernet_frame_size);
			amc_message_send(NET_SERVICE_NAME, packet_msg, amc_message_size);
			free(packet_msg);

			state->rx_curr_buf_off = (state->rx_curr_buf_off + packet_len + 4 + 3) & ~3;
		}

		if (state->rx_curr_buf_off >= 8192) {
			//printf("rollback\n");
			state->rx_curr_buf_off -= 8192;
		}
		// I don't know why 0x10 is subtracted here, but other drivers do it and it works.
		outw(state->io_base + RTL_REG_RX_CURRENT_ADDR_PACKET_READ, state->rx_curr_buf_off - 0x10);

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
	uintptr_t rx_buffer_size = 8192 + 16 + 1500;
	//amc_physical_memory_region_create(rx_buffer_size, &virt_memory_rx_addr, &phys_memory_rx_addr);
    uintptr_t phys_rx_base = 0;
    uintptr_t virt_rx_base = 0;
    amc_alloc_physical_range(rx_buffer_size, &phys_rx_base, &virt_rx_base);
    printf("Got RX buffer [Phys 0x%p] [Virt 0x%p]\n", (void*)phys_rx_base, (void*)virt_rx_base);

	out_state->receive_buffer_virt = virt_rx_base;
	out_state->receive_buffer_phys = phys_rx_base;
	outl(io_base + RTL_REG_RX_BUFFER_PHYS_START, phys_rx_base);

    uintptr_t phys_tx_base = 0;
	uintptr_t virt_tx_base = 0;
    amc_alloc_physical_range((1024 * 8) + 16, &phys_tx_base, &virt_tx_base);
    printf("Got TX buffer [Phys 0x%p] [Virt 0x%p]\n", (void*)phys_tx_base, (void*)virt_tx_base);
	out_state->transmit_buffer_virt = virt_tx_base;
	out_state->transmit_buffer_phys = phys_tx_base;
	outl(io_base + RTL_REG_TX_0_PHYS_START, phys_tx_base);
	outl(io_base + RTL_REG_TX_1_PHYS_START, phys_tx_base);
	outl(io_base + RTL_REG_TX_2_PHYS_START, phys_tx_base);
	outl(io_base + RTL_REG_TX_3_PHYS_START, phys_tx_base);
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
    printf("send_packet(%p, %p, %p)\n", nic_state, packet_data, packet_len);
	uint8_t* buffer = (uint8_t*)nic_state->transmit_buffer_virt;
    printf("buffer %p\n", buffer);
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

static rtl8139_state_t nic_state = {0};

static void _int_received(uint32_t int_no) {
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
	adi_send_eoi(INT_VECTOR_APIC_11);
}

static void _message_received(amc_message_t* msg) {
	const char* source_service = msg->source;
	if (!strncmp(source_service, NET_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
		net_message_t* net_msg = (net_message_t*)msg->body;
		uint32_t event = net_msg->event;
		if (event == NET_TX_ETHERNET_FRAME) {
			uint8_t* packet_data = (uint8_t*)&net_msg->m.packet.data;
			printf("NIC transmitting frame, len = %d data 0x%p\n", net_msg->m.packet.len, packet_data);
			send_packet(&nic_state, packet_data, net_msg->m.packet.len);
		}
		else if (event == NET_REQUEST_NIC_CONFIG) {
			printf("NIC servicing net's NIC config request\n");

			uint32_t mac_part1 = inl(nic_state.io_base + 0x00);
			uint16_t mac_part2 = inw(nic_state.io_base + 0x04);
			uint8_t mac_addr[8];
			mac_addr[0] = mac_part1 >> 0;
			mac_addr[1] = mac_part1 >> 8;
			mac_addr[2] = mac_part1 >> 16;
			mac_addr[3] = mac_part1 >> 24;
			mac_addr[4] = mac_part2 >> 0;
			mac_addr[5] = mac_part2 >> 8;

			net_message_t config_msg;
			config_msg.event = NET_RESPONSE_NIC_CONFIG;
			memcpy(&config_msg.m.config_info.mac_addr, mac_addr, 6);

			amc_message_send(NET_SERVICE_NAME, &config_msg, sizeof(net_nic_config_info_t));
		}
		else {
			printf("Unknown event from net service: %d\n", event);
		}
	}
	else {
		printf("RTL8139 driver got command from unknown service %s\n", source_service);
	}
}

int main(int argc, char** argv) {
	amc_register_service(RTL8139_SERVICE_NAME);
	// This process will handle interrupts from the Realtek 8159 NIC (IRQ11)
	// TODO(PT): The interrupt number is read from the PCI bus
	// It should be communicated to this process
	adi_register_driver(RTL8139_SERVICE_NAME, INT_VECTOR_APIC_11);

	// TODO(PT): This should be read from the PCI bus
	int io_base = 0xc000;
	realtek_8139_init(0, 3, 0, io_base, &nic_state);

	// Set up the event loop with libgui
	gui_application_create();
	gui_add_interrupt_handler(INT_VECTOR_APIC_11, _int_received);
	gui_add_message_handler(_message_received);
	gui_enter_event_loop();
	
	return 0;
}
