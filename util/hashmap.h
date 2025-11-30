#include <stdint.h>

struct HashEntry {
    uint32_t key;
    uint16_t value;
    uint8_t occupied;
};

struct HashMap {
    struct HashEntry* map;
    uint32_t capacity;
    uint32_t keys;
    uint8_t destroyed;
};

struct HashMap* bw_newhashmap(uint32_t capacity);
uint8_t bw_hashset(struct HashMap* map, uint32_t key, uint16_t value);
uint16_t bw_hashget(struct HashMap* map, uint32_t key);
uint8_t bw_hashincrement(struct HashMap* map, uint32_t key);
uint8_t bw_hashresize(struct HashMap* map, uint32_t capacity);
uint8_t bw_hashcompact(struct HashMap* map);
uint8_t bw_hashsort(struct HashMap* map);
uint8_t bw_hashfree(struct HashMap* map);
