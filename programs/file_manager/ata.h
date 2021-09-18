#ifndef ATA_CLIENT_H
#define ATA_CLIENT_H

#include <stdint.h>
#include <drivers/ata/ata_driver_messages.h>

typedef struct ata_sector {
	uint32_t size;
	uint8_t data[];
} ata_sector_t;

ata_sector_t* ata_read_sector(uint32_t sector_lba);
ata_sector_t* ata_write_sector(uint32_t sector_lba, uint8_t* sector_data);

#endif