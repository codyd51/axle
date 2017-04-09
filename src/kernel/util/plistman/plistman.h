#ifndef PLIST_MAN_H
#define PLIST_MAN_H

#include <kernel/util/vfs/fs.h>

typedef struct plist_t {
	int key_count;
	char keys[16][64];
	char vals[16][64];
} plist_t;

void plist_parse(plist_t* plist, FILE* file);
char* plist_val_for_key(plist_t* plist, char* key);
char* plist_key_for_val(plist_t* plist, char* val);

#endif
