#ifndef SATA_DRIVER_MESSAGES_H
#define SATA_DRIVER_MESSAGES_H

#include <stdint.h>

// PT: Add more definitions here as C clients need them

#define SATA_DRIVER_SERVICE_NAME "com.axle.sata_driver"

// TODO(PT): Flesh this out

#define SATA_DRIVER_READ_SECTOR_EVENT 100
typedef struct sata_driver_read {
    uint64_t start_sector;
    uint64_t sector_count;
} sata_driver_read_t;

#endif
