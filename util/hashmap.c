#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "hashmap.h"

uint32_t bw_hashint32(uint32_t x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x);
    return x;
}

struct HashMap* bw_newhashmap(uint32_t capacity) {
    struct HashEntry* map = malloc(capacity * sizeof(struct HashEntry));
    if (!map) return NULL;
    memset(map, 0, capacity * sizeof(struct HashEntry));
    struct HashMap* result = malloc(sizeof(struct HashMap));
    if (!result) {
        free(map);
        return NULL;
    }
    result->map = map;
    result->capacity = capacity;
    result->keys = 0;
    result->destroyed = 0;
    return result;
}

uint8_t bw_hashset_internal(struct HashEntry* mapptr, uint32_t capacity, uint32_t key, uint16_t value) {
    uint32_t hash = bw_hashint32(key) % capacity;
    uint32_t start_hash = hash;

    while (1) {
        struct HashEntry* entry = &mapptr[hash];

        if (entry->occupied == 0) {
            entry->key = key;
            entry->value = value;
            entry->occupied = 1;
            return 2;
        }

        if (entry->occupied == 1 && entry->key == key) {
            entry->value = value;
            return 1;
        }

        hash++;
        if (hash >= capacity) hash = 0;
        if (hash == start_hash) return 0;
    }
}

uint16_t bw_hashget(struct HashMap* map, uint32_t key) {
    uint32_t hash = bw_hashint32(key) % map->capacity;
    uint32_t start_hash = hash;

    while (1) {
        struct HashEntry* entry = &map->map[hash];

        if (entry->occupied == 0) {
            return 0;
        }

        if (entry->occupied == 1 && entry->key == key) {
            return entry->value;
        }

        hash++;
        if (hash >= map->capacity) hash = 0;
        if (hash == start_hash) return 0;
    }
}

uint8_t bw_hashset(struct HashMap* map, uint32_t key, uint16_t value) {
    if (map->destroyed) return 0;

    if ((map->keys * 100 / map->capacity) > 70) {
        bw_hashresize(map, map->capacity * 2);
    }

    uint8_t res = bw_hashset_internal(map->map, map->capacity, key, value);
    if (res == 2) {
        map->keys++;
    }
    return res;
}

uint8_t bw_hashincrement(struct HashMap* map, uint32_t key) {
    uint32_t hash = bw_hashint32(key) % map->capacity;
    uint32_t start_hash = hash;

    while (1) {
        struct HashEntry* entry = &map->map[hash];

        if (entry->occupied == 0) {
            return bw_hashset(map, key, 1);
        }

        if (entry->occupied == 1 && entry->key == key) {
            entry->value++;
            return 1;
        }

        hash++;
        if (hash >= map->capacity) hash = 0;
        if (hash == start_hash) break;
    }

    return bw_hashset(map, key, 1);
}

uint8_t bw_hashresize(struct HashMap* map, uint32_t capacity) {
    struct HashEntry* newmap = malloc(capacity * sizeof(struct HashEntry));
    if (!newmap) return 0;
    memset(newmap, 0, capacity * sizeof(struct HashEntry));

    for (uint32_t idx = 0; idx < map->capacity; idx++) {
        struct HashEntry* oldentry = &map->map[idx];
        if (oldentry->occupied == 1) {
            bw_hashset_internal(newmap, capacity, oldentry->key, oldentry->value);
        }
    }

    free(map->map);
    map->map = newmap;
    map->capacity = capacity;
    return 1;
}

uint8_t bw_hashcompact(struct HashMap* map) {
    map->destroyed = 1;
    uint32_t w = 0;

    for (uint32_t r = 0; r < map->capacity; r++) {
        if (map->map[r].occupied == 1) {
            if (w != r) {
                map->map[w] = map->map[r];
            }
            w++;
        }
    }

    return 1;
}

int cmp_desc(const void* a, const void* b) {
    const struct HashEntry* x = a;
    const struct HashEntry* y = b;
    if (x->value < y->value) return 1;
    if (x->value > y->value) return -1;
    return 0;
}

uint8_t bw_hashsort(struct HashMap* map) {
    map->destroyed = 1;
    bw_hashcompact(map);
    qsort(map->map, map->keys, sizeof(struct HashEntry), cmp_desc);
    return 1;
}

uint8_t bw_hashfree(struct HashMap* map) {
    free(map->map);
    free(map);
    return 1;
}
