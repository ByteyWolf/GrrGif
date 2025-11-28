#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hashmap.h"

uint32_t bw_hashint32(uint32_t x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x);
    return x;

struct HashMap* bw_newhashmap(uint32_t capacity) {
    struct HashEntry* map = malloc(capacity * sizeof(struct HashEntry));
    if (!map) return 0;
    memset(map, 0, capacity * sizeof(struct HashEntry));
    struct HashMap* result = malloc(sizeof(struct HashMap));
    if (!result) {free(map); return 0;}
    result->map = map;
    result->capacity = capacity;
    result->keys = 0;
    result->destroyed = 0;
    return result;
}

uint8_t bw_hashset_internal(struct HashEntry* mapptr, uint32_t capacity, uint32_t key, uint16_t value) {
    uint32_t hash = bw_hashint32(key) % capacity;
    struct HashEntry entry = 0;
    while (1) {
        entry = map[hash];
        if (entry.occupied == 0) break;
        if (entry.occupied == 1 && entry.key == key) break;
        hash++;
        if (hash >= capacity) return 0;
    }
    if (!entry) return 0;
    entry.value = value;
    if (entry.occupied == 0) {
        entry.key = key;
        entry.occupied = 1;
        return 2;
    }
    return 1;
}

uint16_t bw_hashget(struct HashEntry* map, uint32_t key) {
    uint32_t hash = bw_hashint32(key) % capacity;
    struct HashEntry entry = 0;
    while (1) {
        entry = map[hash];
        if (entry.occupied == 0) break;
        if (entry.occupied == 1 && entry.key == key) break;
        hash++;
        if (hash >= capacity) return 0;
    }
    if (!entry) return 0;
    return entry.value;
}

uint8_t bw_hashset(struct HashMap* map, uint32_t key, uint16_t value) {
    if (map->destroyed) return 0;
    if ((map->keys * 100 / map->capacity) > 70)
        bw_hashresize(map, map->capacity * 2);
    uint8_t res = bw_hashset_internal(map->map, map->capacity, key, value);
    if (res == 2)
        map->keys++;
    return res;
}

uint8_t bw_hashincrement(struct HashEntry* map, uint32_t key) {
    uint32_t hash = bw_hashint32(key) % capacity;
    struct HashEntry entry = 0;
    while (1) {
        entry = map[hash];
        if (entry.occupied == 0) break;
        if (entry.occupied == 1 && entry.key == key) break;
        hash++;
        if (hash >= capacity) break;
    }
    if (!entry) return bw_hashset(map, key, 1);
    entry.value++;
    return 1;
}

uint8_t bw_hashresize(struct HashMap* map, uint32_t capacity) {
    struct HashEntry* newmap = malloc(capacity * sizeof(struct HashEntry));
    if (!newmap) return 0;
    memset(newmap, 0, capacity * sizeof(struct HashEntry));

    for (uint32_t idx = 0; idx < map->capacity; idx++) {
        struct HashEntry oldkey = map->map[idx];
        bw_hashset_internal(newmap, capacity, oldkey.key, oldkey.value);
    }

    map->capacity = capacity;
    map->map = newmap;
    return 1;
}

uint8_t bw_hashcompact(struct HashMap* map) {
    map->destroyed = 1;
    uint32_t w = 0;
    for (uint32_t r = 0; r < map->capacity; r++) {
        if (map->map[r].occupied == 1 && w != r) {
            map->map[w] = map->map[r];
        }
        w++;
    }
    return 1;
}

int cmp_desc(const void* a, const void* b) {
    const struct HashEntry* x = a;
    const struct HashEntry* y = b;
    if (x->count < y->count) return 1;
    if (x->count > y->count) return -1;
    return 0;
}

uint8_t bw_hashsort(struct HashMap* map) {
    map->destroyed = 1;
    bw_compact(map);
    qsort(map->map, map->keys, sizeof(struct HashEntry), cmp_desc);
    return 1;
}

uint8_t bw_hashfree(struct HashMap* map) {
    free(map->map);
    free(map);
    return 1;
}
