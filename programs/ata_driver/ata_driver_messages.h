#ifndef ATA_DRIVER_MESSAGES_H
#define ATA_DRIVER_MESSAGES_H

#include <stdint.h>

#define ATA_DRIVER_SERVICE_NAME "com.axle.ata"

typedef enum ata_drive {
	ATA_DRIVE_MASTER = 0,
	ATA_DRIVE_SLAVE = 1
} ata_drive_t;

// Sent from clients to the ATA driver
#define ATA_READ_REQUEST 100
typedef struct ata_read_sector_request {
    uint32_t event; // ATA_READ_REQUEST
    ata_drive_t drive_desc;
    uint32_t sector;
} ata_read_sector_request_t;

// Sent from the ATA driver to clients
#define ATA_READ_RESPONSE 100
typedef struct ata_read_sector_response {
    uint32_t event; // ATA_READ_RESPONSE
    ata_drive_t drive_desc;
    uint32_t sector;
    uint32_t sector_size;
    uint8_t sector_data[];
} ata_read_sector_response_t;

#endif
