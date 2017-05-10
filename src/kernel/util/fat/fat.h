#ifndef FAT_H
#define FAT_H

#include <stdint.h>
#include <std/std.h>

/*!
 * @brief Calculate hard drive sectors needed to store @p bytes 
 */
int sectors_from_bytes(int bytes);

/*!
 * @brief Retrieve current filesystem's FAT
 */
uint32_t* fat_get();

/*!
 * @brief Retrieve size of a hard drive sector from current FAT
 */
int fat_read_sector_size();

/*!
 * @brief Retrieve number of hard drive sectors tracked by current FAT
 */
int fat_read_sector_count();

/*!
 * @brief Print all sectors attatched to a file starting at @p sector
 */
void fat_print_file_links(uint32_t sector);

/*!
 * @brief Register a new file with the current FAT
 * @return The first sector of the newly created file
 */
int fat_file_create();

/*!
 * @brief Expand @p file by @p byte_count
 * @param file The first sector of the file to expand
 * @param byte_count The number of bytes to increase the file
 */
void fat_expand_file(uint32_t file, uint32_t byte_count);

/*!
 * @brief Shrink @p file by @byte_count
 * Deallocates sectors required to contain @p byte_count, 
 * starting from the end of the file.
 * @param file The first sector of the file to shrink
 * @param byte_count The number of bytes to shrink the file
 */
void fat_shrink_file(uint32_t file, uint32_t byte_count);

/*!
 * @brief Format the IDE ATA drive @p drive with a FAT filesystem.
 * This function also sets the newly formatted FAT as the active filesystem.
 * @param drive The connected drive number, from 0-3, to format with FAT.
 */
void fat_format_disk(unsigned char drive);

/*!
 * @brief Write contents of current FAT to physical disk.
 */
void fat_flush();

#endif
