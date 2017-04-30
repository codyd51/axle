#ifndef HDD_FS_H
#define HDD_FS_H

void hdd_read(int fileno, char* buf, int count);
void hdd_write(int fileno, char* buf, int count);
int hdd_file_create();

#endif
