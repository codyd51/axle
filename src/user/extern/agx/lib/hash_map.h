#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <stdint.h>

typedef struct hash_map hash_map_t;

hash_map_t* hash_map_create(uint32_t initial_size);
void hash_map_put(hash_map_t* map, void* key_buf, uint32_t key_buf_len, void* value);
void* hash_map_get(hash_map_t* map, void* key_buf, uint32_t key_buf_len);
uint32_t hash_map_size(hash_map_t* map);

#endif