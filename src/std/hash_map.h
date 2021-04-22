#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <stdint.h>

typedef struct hash_map hash_map_t;

hash_map_t* hash_map_create(void);
void hash_map_put(hash_map_t* map, void* key_buf, uint32_t key_buf_len, void* value);
void* hash_map_get(hash_map_t* map, void* key_buf, uint32_t key_buf_len);

#endif