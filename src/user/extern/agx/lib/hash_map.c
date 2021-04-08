#include <stdlib.h>
#include <stdbool.h>

#include "hash_map.h"
#include <stdlibadd/assert.h>

uint32_t hash(const uint8_t* key, size_t length) {
    // https://en.wikipedia.org/wiki/Jenkins_hash_function
    size_t i = 0;
    uint32_t hash = 0;
    while (i != length) {
        hash += key[i++];
        hash += hash << 10;
        hash ^= hash >> 6;
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}

typedef struct hash_map_elem {
    bool in_use;
    uint32_t hash;
    void* value;
    struct hash_map_elem* next;
} hash_map_elem_t;

typedef struct hash_map {
    hash_map_elem_t* data;
    uint32_t data_size;
} hash_map_t;

hash_map_t* hash_map_create(void) {
    hash_map_t* map = calloc(1, sizeof(hash_map_t));
    map->data_size = 512;
    map->data = calloc(map->data_size, sizeof(hash_map_elem_t));
    //map->data = malloc(512 * sizeof(hash_map_elem_t));
    //printf("hash_map_create 0x%08x data 0x%08x\n", map, map->data);
    //memset(map->data, 0, sizeof(hash_map_elem_t) * map->data_size);
}

void hash_map_put(hash_map_t* map, void* key_buf, uint32_t key_buf_len, void* value, bool d) {
    uint32_t h = hash(key_buf, key_buf_len);
    uint32_t idx = h % map->data_size;
    hash_map_elem_t* elem = &(map->data[idx]);
    //if (d) printf("put h %u idx %d elem 0x%08x %c %d\n", h, idx, elem, ((char*)(key_buf))[0], elem->in_use);
    //assert(!elem->in_use, "hash collision");
    if (elem->in_use) {
        // Find the end of the linked list
        hash_map_elem_t* iter = elem;
        while (iter->next != NULL) {
            iter = iter->next;
        }
        hash_map_elem_t* new = calloc(1, sizeof(hash_map_elem_t));
        iter->next = new;
        new->in_use = true;
        new->hash = h;
        new->value = value;
        return;
    }
    elem->in_use = true;
    elem->hash = h;
    elem->value = value;
}

void* hash_map_get(hash_map_t* map, void* key_buf, uint32_t key_buf_len, bool d) {
    uint32_t h = hash(key_buf, key_buf_len);
    uint32_t idx = h % map->data_size;
    hash_map_elem_t* elem = &map->data[idx];
    //if (d) printf("get h %u idx %d elem 0x%08x %c %d\n", h, idx, elem, ((char*)(key_buf))[0], elem->in_use);
    if (!elem->in_use) {
        return NULL;
    }

    if (elem->hash != h) {
        // Iterate the linked list here
        hash_map_elem_t* iter = elem;
        while (iter != NULL) {
            if (iter->in_use && iter->hash == h) {
                return iter->value;
            }
            iter = iter->next;
        }
        return NULL;
    }

    return elem->value;
}