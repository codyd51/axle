#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <kernel/adi.h>
#include <kernel/amc.h>
#include <kernel/idt.h>

#include <libgui/libgui.h>

// PCI
#include <pci/pci_messages.h>

// IPC communication
#include <libamc/libamc.h>

// Port IO
#include <libport/libport.h>

#include <stdlibadd/assert.h>

#include "ata_driver_messages.h"
#include "ata_driver.h"

#define IO_PORT_BASE 0x1F0
#define CONTROL_PORT_BASE 0x3F6

// http://users.utcluj.ro/~baruch/media/siee/labor/ATA-Interface.pdf

#define ATA_REG_RW__SECTOR_COUNT (IO_PORT_BASE + 2)

#define ATA_REG_RW__LBA_LOW (IO_PORT_BASE + 3)
#define ATA_REG_RW__LBA_MID (IO_PORT_BASE + 4)
#define ATA_REG_RW__LBA_HIGH (IO_PORT_BASE + 5)

/*
+---+-----+---+-----+---+---+---+---+
| 7 | 6   | 5 | 4   | 3 | 2 | 1 | 0 |
+---+-----+---+-----+---+---+---+---+
| X | LBA | X | Dev | ? | ? | ? | ? |
+---+-----+---+-----+---+---+---+---+
*/
#define ATA_REG_RW__DRIVE_HEAD (IO_PORT_BASE + 6)

/*
+------+--------+----------+---+---------+---+---+--------+
| 7    | 6      | 5        | 4 | 3       | 2 | 1 | 0      |
+------+--------+----------+---+---------+---+---+--------+
| Busy | DevRdy | DevFault | # | DataReq | X | X | ErrChk |
+------+--------+----------+---+---------+---+---+--------+
*/
#define ATA_REG_R__STATUS (IO_PORT_BASE + 7)

#define ATA_REG_W__COMMAND (IO_PORT_BASE + 7)

#define ATA_CMD__DEVICE_RESET	0x08
#define ATA_CMD__READ_SECTORS	0x20

#define ATA_SECTOR_SIZE 512

typedef struct ata_queued_read {
    const char source_service[AMC_MAX_SERVICE_NAME_LEN];
	ata_drive_t drive_desc;
	uint32_t sector;
} ata_queued_read_t;

typedef struct ata_read_request_state {
	// Operates FIFO, popping from the bottom
	array_t* queued_reads;
} ata_driver_state_t;

static ata_driver_state_t* _state = NULL;

static uint8_t ata_status(void) {
	return inb(ATA_REG_R__STATUS);
}

static void ata_write_command(uint8_t command) {
	return outb(ATA_REG_W__COMMAND, command);
}

static void ata_delay(void) {
	// Sleep 1ms
	uint32_t b[2];
    b[0] = AMC_SLEEP_UNTIL_TIMESTAMP;
    b[1] = 1;
    amc_message_construct_and_send(AXLE_CORE_SERVICE_NAME, &b, sizeof(b));
}

static void ata_select_drive(ata_drive_t drive) {
	const char* name = drive == ATA_DRIVE_MASTER ? "master" : "slave";
	printf("\tStatus register before [ATA %s] select: 0x%02x\n", name, ata_status());
	printf("Select [ATA %s]...\n", name);

	// TODO(PT): Read instead of writing, clear the specific bit, then re-write
	outb(ATA_REG_RW__DRIVE_HEAD, (drive << 4));

	ata_delay();

	printf("\tStatus register after [ATA %s] select: 0x%02x\n", name, ata_status());
}

static void ata_read_sectors(uint32_t lba, uint16_t sector_count) {
	if (sector_count >= 256) {
		sector_count = 0;
	}
	outb(
		ATA_REG_RW__DRIVE_HEAD,
		(
			(1 << 6) |	// LBA addressing mode
			(ATA_DRIVE_MASTER << 4) |	// Drive select
			((lba >> 24) & 0x0F)	// Top 3 bits of LBA
		)
	);
	ata_delay();
	printf("\tStatus register after write LBA descriptors: 0x%02x\n", ata_status());

	outb(ATA_REG_RW__SECTOR_COUNT, (uint8_t)sector_count);
	outb(ATA_REG_RW__LBA_LOW, (uint8_t)lba);
	outb(ATA_REG_RW__LBA_MID, (uint8_t)(lba >> 8));
	outb(ATA_REG_RW__LBA_HIGH, (uint8_t)(lba >> 16));
	outb(ATA_REG_W__COMMAND, ATA_CMD__READ_SECTORS);
}

static void ata_init(void) {
    printf("Init ATA driver\n");
	printf("Status register: 0x%02x\n", inb(ATA_REG_R__STATUS));

	_state = calloc(1, sizeof(ata_driver_state_t));
	_state->queued_reads = array_create(32);

	ata_select_drive(ATA_DRIVE_MASTER);
	ata_delay();
	printf("\tStatus register after device select: 0x%02x\n", ata_status());
}

static void _int_received(uint32_t int_no) {
	printf("ATA received interrupt! %ld\n", int_no);
	printf("Status: 0x%02x\n", ata_status());

	ata_queued_read_t* queued_read = array_lookup(_state->queued_reads, 0);
	array_remove(_state->queued_reads, 0);

	uint32_t response_size = sizeof(ata_read_sector_response_t) + ATA_SECTOR_SIZE;
	ata_read_sector_response_t* response = calloc(1, response_size);
	response->event = ATA_READ_RESPONSE;
	response->drive_desc = queued_read->drive_desc;
	response->sector = queued_read->sector;
	response->sector_size = ATA_SECTOR_SIZE;

	for (uint32_t i = 0; i < ATA_SECTOR_SIZE; i += sizeof(uint16_t)) {
		response->sector_data[i] = inw(0x1F0);
		printf("ATA read val (i = %ld): 0x%04x\n", i, response->sector_data[i]);
	}
	adi_send_eoi(int_no);

	amc_message_construct_and_send(queued_read->source_service, response, response_size);
	free(queued_read);
	free(response);
}

static void _message_received(amc_message_t* msg) {
	printf("[ATA] Received message from %s\n", msg->source);

	ata_read_sector_request_t* read_request = (ata_read_sector_request_t*)&msg->body;
	assert(read_request->event == ATA_READ_REQUEST, "Expected read sector request");
	assert(read_request->drive_desc == ATA_DRIVE_MASTER, "Only the master drive is currently supported");

	printf("[ATA] %s requested read of sector %ld\n", msg->source, read_request->sector);

	ata_queued_read_t* queued_read = calloc(1, sizeof(ata_queued_read_t));
	queued_read->drive_desc = read_request->drive_desc;
	queued_read->sector = read_request->sector;
	strncpy(queued_read->source_service, msg->source, AMC_MAX_SERVICE_NAME_LEN);
	array_insert(_state->queued_reads, queued_read);
	ata_read_sectors(queued_read->sector, 1);
}

int main(int argc, char** argv) {
	amc_register_service(ATA_DRIVER_SERVICE_NAME);
	adi_register_driver(ATA_DRIVER_SERVICE_NAME, INT_VECTOR_IRQ14);

	ata_init();

	// Set up the event loop with libgui
	gui_application_create();
	gui_add_interrupt_handler(INT_VECTOR_IRQ14, _int_received);
	gui_add_message_handler(_message_received);
	gui_enter_event_loop();
	
	return 0;
}
