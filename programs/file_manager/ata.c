#include <stddef.h>
#include <libamc/libamc.h>

#include "ata.h"

ata_sector_t* ata_read_sector(uint32_t sector_lba) {
	ata_read_sector_request_t read = {0};
	read.event = ATA_READ_REQUEST;
	read.drive_desc = ATA_DRIVE_MASTER;
	read.sector = sector_lba;
	amc_message_construct_and_send(ATA_DRIVER_SERVICE_NAME, &read, sizeof(ata_read_sector_request_t));
	printf("[FS] Reading sector %d from ATA driver...\n", sector_lba);

	amc_message_t* response_msg;
	amc_message_await(ATA_DRIVER_SERVICE_NAME, &response_msg);
	printf("[FS] Received response from ATA driver\n");
	ata_read_sector_response_t* response = (ata_read_sector_response_t*)&response_msg->body;
    printf("[FS] Response %ld\n", response->sector_size);
    hexdump(response->sector_data, 128);

	ata_sector_t* sector = calloc(1, sizeof(ata_sector_t) + response->sector_size);
	sector->size = response->sector_size;
	memcpy(sector->data, response->sector_data, response->sector_size);
	return sector;
}

ata_sector_t* ata_write_sector(uint32_t sector_lba, uint8_t* sector_data) {
	// TODO(PT): Define/pull this?
	uint32_t sector_size = 512;
	uint32_t request_size = sizeof(ata_write_sector_request_t) + sector_size;
	ata_write_sector_request_t* write = calloc(1, request_size);
	write->event = ATA_WRITE_REQUEST;
	write->drive_desc = ATA_DRIVE_MASTER;
	write->sector = sector_lba;
	memcpy(write->sector_data, sector_data, sector_size);
	amc_message_construct_and_send(ATA_DRIVER_SERVICE_NAME, write, request_size);
	free(write);
	printf("[FS] Writing sector %d to ATA driver...\n", sector_lba);
}

// TODO(PT): Copied from net/utils, refactor elsewhere?
void hexdump(const void* addr, const int len) {
    const char* desc = "";
    int i;
    unsigned char buff[65];
    const unsigned char * pc = (const unsigned char *)addr;

    // Output description if given.

    if (desc != NULL)
        printf ("%s:\n", desc);

    // Length checks.

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    else if (len < 0) {
        printf("  NEGATIVE LENGTH: %d\n", len);
        return;
    }

    // Process every byte in the data.

    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 64) == 0) {
            // Don't print ASCII buffer for the "zeroth" line.

            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.

            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf ("%02x ", pc[i]);

        // And buffer a printable ASCII character for later.

        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) // isprint() may be better.
            buff[i % 64] = '.';
        else
            buff[i % 64] = pc[i];
        buff[(i % 64) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.

    while ((i % 64) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII buffer.

    printf ("  %s\n", buff);
}
