#ifndef ATA_DRIVER_MESSAGES_H
#define ATA_DRIVER_MESSAGES_H

#include <stdint.h>

#define ATA_DRIVER_SERVICE_NAME "com.axle.ata"

typedef enum ata_drive {
	ATA_DRIVE_MASTER = 0,
	ATA_DRIVE_SLAVE = 1
} ata_drive_t;

typedef struct ata_message_base {
    uint32_t event;
} ata_message_base_t;

// Sent from clients to the ATA driver
// PT: Chosen to not conflict with file_manager messages
#define ATA_READ_REQUEST 300
typedef struct ata_read_sector_request {
    uint32_t event; // ATA_READ_REQUEST
    ata_drive_t drive_desc;
    uint32_t sector;
} ata_read_sector_request_t;

// Sent from the ATA driver to clients
#define ATA_READ_RESPONSE 300
typedef struct ata_read_sector_response {
    uint32_t event; // ATA_READ_RESPONSE
    ata_drive_t drive_desc;
    uint32_t sector;
    uint32_t sector_size;
    uint8_t sector_data[];
} ata_read_sector_response_t;

// Sent from clients to the ATA driver
#define ATA_WRITE_REQUEST 301
typedef struct ata_write_sector_request {
    uint32_t event; // ATA_WRITE_REQUEST
    ata_drive_t drive_desc;
    uint32_t sector;
    uint8_t sector_data[];
} ata_write_sector_request_t;

typedef union ata_message {
    ata_message_base_t base;
    ata_read_sector_request_t read;
    ata_write_sector_request_t write;
} ata_message_t;

#endif
