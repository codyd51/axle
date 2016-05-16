#ifndef COMMON_H
#define COMMON_H

//standardize sizes across platforms
typedef unsigned int   u32int;
typedef 	 int   s32int;
typedef unsigned short u16int;
typedef 	 short s16int;
typedef unsigned char  u8int;
typedef 	 char  s8int;

void outb(u16int port, u8int val);
u8int inb(u16int port);
u16int inw(u16int port);

#endif
