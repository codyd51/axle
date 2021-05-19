#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "memory.h"
#include "hash_map.h"

#include <kernel/assert.h>

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
    printf("hash_map_create\n");
    hash_map_t* map = calloc(1, sizeof(hash_map_t));
    map->data_size = 512;
    map->data = calloc(map->data_size, sizeof(hash_map_elem_t));
    //map->data = malloc(512 * sizeof(hash_map_elem_t));
    //printf("hash_map_create 0x%08x data 0x%08x\n", map, map->data);
    //memset(map->data, 0, sizeof(hash_map_elem_t) * map->data_size);
}

void hash_map_put(hash_map_t* map, void* key_buf, uint32_t key_buf_len, void* value) {
    uint32_t h = hash(key_buf, key_buf_len);
    uint32_t idx = h % map->data_size;
    hash_map_elem_t* elem = &(map->data[idx]);
    //assert(!elem->in_use, "hash collision");
    //if (elem->in_use) {
        // Find the end of the linked list
        hash_map_elem_t* iter = elem;
        while (iter->next != NULL) {
            iter = iter->next;
        }
        assert(iter->next == NULL, "not null");
        hash_map_elem_t* new = calloc(1, sizeof(hash_map_elem_t));
        iter->next = new;
        printf("hash_map calloc, elem 0x%08x iter 0x%08x new 0x%08x map 0x%08x key 0x%08x len %d value 0x%08x h %d idx %d!\n", elem, iter, new, map, key_buf, key_buf_len, value, h, idx);
        new->in_use = true;
        new->hash = h;
        new->value = value;
        return;
    //}
    elem->in_use = true;
    elem->hash = h;
    elem->value = value;
}

void hash_map_delete(hash_map_t* map, void* key_buf, uint32_t key_buf_len) {
    uint32_t h = hash(key_buf, key_buf_len);
    uint32_t idx = h % map->data_size;
    hash_map_elem_t* elem = &(map->data[idx]);

    printf("hash_map_delete 0x%08x 0x%08x key_buf_len: %d h: %d idx: %d\n", map, key_buf, key_buf_len, h, idx);

    //if (elem->in_use) {
        // Find the end of the linked list
        hash_map_elem_t* head = elem;
        hash_map_elem_t* iter = head;
        do {
            if (iter->hash == h) {
                printf("Found elem in hash map elem 0x%08x iter 0x%08x hash 0x%08x\n", elem, iter, h);
                //if (iter == elem) {
                    iter->in_use = false;
                    //memset(iter, 0, sizeof(hash_map_elem_t));
                //}
                /*
                // We could eventually free the node
                else {
                    printf("Failed to find item notimpl\n");
                    NotImplemented();
                }
                */
                return;
            }
            iter = iter->next;
        } while (iter != NULL);
        printf("Failed to find item?\n");
    //}
}

void* hash_map_get(hash_map_t* map, void* key_buf, uint32_t key_buf_len) {
    uint32_t h = hash(key_buf, key_buf_len);
    uint32_t idx = h % map->data_size;
    hash_map_elem_t* elem = &map->data[idx];
    /*
    if (!elem->in_use) {
        printf("hash_map_get failed, in_use = false\n");
        return NULL;
    }
    */

    //if (elem->hash != h) {
        printf("hash_map_get iterate linked list\n");
        // Iterate the linked list here
        hash_map_elem_t* head = elem;
        hash_map_elem_t* iter = head;
        do {
            printf("\titer 0x%08x hash %d next 0x%08x\n", iter, iter->hash, iter->next);
            if (iter->hash == h && iter->in_use) {
                printf("GET --- Found elem in hash map elem 0x%08x iter 0x%08x hash 0x%08x\n", elem, iter, h);
                assert(iter->in_use, "not in use");
                return iter->value;
            }
            iter = iter->next;
        } while (iter != NULL);
        /*
        hash_map_elem_t* iter = elem;
        while (iter != NULL) {
            printf("iter 0x%08x %d %d, %d\n", iter, iter->in_use, iter->hash, h);
            if (iter->in_use && iter->hash == h) {
                return iter->value;
            }
            iter = iter->next;
        }
        */
        return NULL;
    //}

    return elem->value;
}
