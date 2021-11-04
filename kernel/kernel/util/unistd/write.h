#ifndef WRITE_H
#define WRITE_H

int write(int fd, char* buf, int len);
int std_write(void* task, int fd, const void* buf, int len);

#endif
