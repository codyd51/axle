#ifndef FAT_H
#define FAT_H

#include <stdint.h>
#include <std/std.h>
#include <kernel/util/multitasking/fd.h>
#include <kernel/util/vfs/fs.h>

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
 * @param file_size Size to allocate initially, in bytes
 * @return The first sector of the newly created file
 */
int fat_file_create(int file_size);

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

/*!
 * @brief Add the directory entry @p new_entry to the directory represented by the sector @p directory_sector
 * @param directory_sector The sector where the directory to append to begins
 * @param new_entry Struct containing the new directory entry info to add
 */
void fat_dir_add_file(fat_dirent* directory, fat_dirent* new_entry);

/*!
 * @brief Find and read from the file located at absolute path @p name
 * @param name The absolute path of the file to find
 * @param buffer Character buffer to store data from file into
 * @param count Number of bytes to read from specified file
 * @param offset Number of bytes in file to skip before beginning to read
 * @warning Traversing up a directory is not currently supported.
 */
int fat_read_absolute_file(char* name, char* buffer, int count, int offset);

/*!
 * @brief Find and write from the file located at absolute path @p name
 * @param name The absolute path of the file to find
 * @param buffer Character buffer to store data from file into
 * @param count Number of bytes to write to specified file
 * @param offset Number of bytes in file to skip before beginning to write
 * @warning Traversing up a directory is not currently supported.
 */
int fat_write_absolute_file(char* name, char* buffer, int count, int offset); 

/*!
 * @brief Attempt to find the FAT index associated with absolute path @p name
 * @param name The absolute path of the entry to search for
 * @param store Optional user-provided buffer to store dirent of the found file
 * @return The index of the first sector of the resource, or -1 if the resource wasn't found.
 */
int fat_find_absolute_file(char* name, fat_dirent* store);

size_t fat_fread(void* ptr, size_t size, size_t count, FILE* stream);
size_t fat_fwrite(void* ptr, size_t size, size_t count, FILE* stream);
FILE* fat_fopen(char* filename, char* mode);

int sectors_from_bytes(int bytes);

#endif
