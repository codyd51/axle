#ifndef FAT_DIRENT_H
#define FAT_DIRENT_H

#include <stdint.h>

/*!
 * @brief Represents a FAT directory entry
 */

typedef struct {
    char name[31];
    char is_directory:1;
    char reserved:3;
    char padded:4;
    uint32_t size;
    uint32_t first_sector;
    uint32_t access_time;
} fat_dirent;

#endif
