#ifndef SHA256_H
#define SHA256_H

#include <std/std.h>
#include <std/memory.h>

#define SHA256_BLOCK_SIZE 32 

typedef unsigned char BYTE;  
typedef unsigned int  WORD;   

typedef struct {
	BYTE data[64];
	WORD datalen;
	unsigned long long bitlen;
	WORD state[8];
} SHA256_CTX;

void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const BYTE data[], size_t len);
void sha256_final(SHA256_CTX *ctx, BYTE hash[]);

int sha256_test();

#endif   // SHA256_H