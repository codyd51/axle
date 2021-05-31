#ifndef LIBPORT_H
#define LIBPORT_H

#include <stdbool.h>

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a <= _b ? _a : _b; })

void assert(bool cond, const char* msg);

void outb(uint16_t port, uint8_t val);
void outw(uint16_t port, uint16_t val);
void outl(uint16_t port, uint32_t val);

uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t inl(uint16_t port);

uint8_t htonb(uint8_t byte, int num_bits);
uint16_t htons(uint16_t hostshort);
uint32_t htonl(uint32_t hostlong);

uint8_t ntohb(uint8_t byte, int num_bits);
uint16_t ntohs(uint16_t netshort);
uint32_t ntohl(uint32_t netlong);

#endif