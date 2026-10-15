/*
 * CherryEBPF - eBPF Map Implementation (Hash, Array, RingBuf)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "ebpf_map.h"
#include "cherry_kal.h"
extern int snprintf(char *str, size_t size, const char *format, ...);

static struct bpf_map *g_map_list = NULL;
static uint32_t g_next_map_id = 1;
static volatile uint32_t g_map_lock = 0;

static inline void map_global_lock(void) {
    while (__atomic_test_and_set(&g_map_lock, __ATOMIC_ACQUIRE)) {
        __asm__ volatile("yield");
    }
}

static inline void map_global_unlock(void) {
    __atomic_clear(&g_map_lock, __ATOMIC_RELEASE);
}

static inline void map_lock(struct bpf_map *map) {
    while (__atomic_test_and_set(&map->lock, __ATOMIC_ACQUIRE)) {
        __asm__ volatile("yield");
    }
}

static inline void map_unlock(struct bpf_map *map) {
    __atomic_clear(&map->lock, __ATOMIC_RELEASE);
}

/* FNV-1a Hash Function */
static uint32_t fnv1a_hash(const void *key, uint32_t len)
{
    uint32_t h = 2166136261u;
    const uint8_t *p = (const uint8_t *)key;
    for (uint32_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

/*
 * Create eBPF Map
 */
struct bpf_map *cherry_ebpf_map_create(const char *name, uint32_t type,
                                       uint32_t key_size, uint32_t value_size,
                                       uint32_t max_entries)
{
    if (max_entries == 0) return NULL;

    struct bpf_map *map = (struct bpf_map *)kmalloc(sizeof(*map), GFP_KERNEL);
    if (!map) return NULL;
    memset(map, 0, sizeof(*map));

    map->type = type;
    map->key_size = key_size;
    map->value_size = value_size;
    map->max_entries = max_entries;

    if (name) {
        strncpy(map->name, name, sizeof(map->name) - 1);
    } else {
        snprintf(map->name, sizeof(map->name), "map_%u", g_next_map_id);
    }

    if (type == BPF_MAP_TYPE_HASH) {
        /* Allocate power of 2 buckets (e.g. 64 or max_entries) */
        uint32_t num_buckets = 16;
        while (num_buckets < max_entries && num_buckets < 512) {
            num_buckets <<= 1;
        }
        map->num_buckets = num_buckets;
        map->buckets = (struct cherry_ebpf_map_elem **)kmalloc(
            sizeof(struct cherry_ebpf_map_elem *) * num_buckets, GFP_KERNEL);
        if (!map->buckets) {
            kfree(map);
            return NULL;
        }
        memset(map->buckets, 0, sizeof(struct cherry_ebpf_map_elem *) * num_buckets);
    } else if (type == BPF_MAP_TYPE_ARRAY) {
        size_t total_sz = (size_t)max_entries * value_size;
        map->storage = kmalloc(total_sz, GFP_KERNEL);
        if (!map->storage) {
            kfree(map);
            return NULL;
        }
        memset(map->storage, 0, total_sz);
        map->cur_entries = max_entries;
    } else if (type == BPF_MAP_TYPE_RINGBUF) {
        /* max_entries specifies ring buffer size in bytes (must be power of 2) */
        uint32_t size = 4096;
        while (size < max_entries && size < 65536) {
            size <<= 1;
        }
        map->rb_size = size;
        map->rb_mask = size - 1;
        map->storage = kmalloc(size, GFP_KERNEL);
        if (!map->storage) {
            kfree(map);
            return NULL;
        }
        memset(map->storage, 0, size);
    }

    map_global_lock();
    map->id = g_next_map_id++;
    map->next = g_map_list;
    g_map_list = map;
    map_global_unlock();

    return map;
}

/*
 * Destroy Map
 */
void cherry_ebpf_map_destroy(struct bpf_map *map)
{
    if (!map) return;

    map_global_lock();
    struct bpf_map **curr = &g_map_list;
    while (*curr) {
        if (*curr == map) {
            *curr = map->next;
            break;
        }
        curr = &(*curr)->next;
    }
    map_global_unlock();

    if (map->type == BPF_MAP_TYPE_HASH && map->buckets) {
        for (uint32_t i = 0; i < map->num_buckets; i++) {
            struct cherry_ebpf_map_elem *elem = map->buckets[i];
            while (elem) {
                struct cherry_ebpf_map_elem *next = elem->next;
                kfree(elem);
                elem = next;
            }
        }
        kfree(map->buckets);
    } else if (map->storage) {
        kfree(map->storage);
    }

    kfree(map);
}

struct bpf_map *cherry_ebpf_map_find_by_id(uint32_t id)
{
    map_global_lock();
    struct bpf_map *m = g_map_list;
    while (m) {
        if (m->id == id) {
            map_global_unlock();
            return m;
        }
        m = m->next;
    }
    map_global_unlock();
    return NULL;
}

struct bpf_map *cherry_ebpf_map_find_by_name(const char *name)
{
    if (!name) return NULL;
    map_global_lock();
    struct bpf_map *m = g_map_list;
    while (m) {
        if (strcmp(m->name, name) == 0) {
            map_global_unlock();
            return m;
        }
        m = m->next;
    }
    map_global_unlock();
    return NULL;
}

/*
 * Map Lookup
 */
void *cherry_ebpf_map_lookup(struct bpf_map *map, const void *key)
{
    if (!map || !key) return NULL;

    if (map->type == BPF_MAP_TYPE_ARRAY) {
        uint32_t idx = *(const uint32_t *)key;
        if (idx >= map->max_entries) return NULL;
        return (uint8_t *)map->storage + ((size_t)idx * map->value_size);
    }

    if (map->type == BPF_MAP_TYPE_HASH) {
        uint32_t hash = fnv1a_hash(key, map->key_size);
        uint32_t bucket = hash % map->num_buckets;

        map_lock(map);
        struct cherry_ebpf_map_elem *cur = map->buckets[bucket];
        while (cur) {
            if (cur->hash == hash && memcmp(cur->data, key, map->key_size) == 0) {
                void *val = cur->data + map->key_size;
                map_unlock(map);
                return val;
            }
            cur = cur->next;
        }
        map_unlock(map);
        return NULL;
    }

    return NULL;
}

/*
 * Map Update
 */
int cherry_ebpf_map_update(struct bpf_map *map, const void *key, const void *value, uint64_t flags)
{
    if (!map || !key || !value) return -1;

    if (map->type == BPF_MAP_TYPE_ARRAY) {
        uint32_t idx = *(const uint32_t *)key;
        if (idx >= map->max_entries) return -2;
        if (flags == BPF_NOEXIST) return -3;
        memcpy((uint8_t *)map->storage + ((size_t)idx * map->value_size), value, map->value_size);
        return 0;
    }

    if (map->type == BPF_MAP_TYPE_HASH) {
        uint32_t hash = fnv1a_hash(key, map->key_size);
        uint32_t bucket = hash % map->num_buckets;

        map_lock(map);
        struct cherry_ebpf_map_elem *cur = map->buckets[bucket];
        while (cur) {
            if (cur->hash == hash && memcmp(cur->data, key, map->key_size) == 0) {
                if (flags == BPF_NOEXIST) {
                    map_unlock(map);
                    return -4; /* Already exists */
                }
                memcpy(cur->data + map->key_size, value, map->value_size);
                map_unlock(map);
                return 0;
            }
            cur = cur->next;
        }

        if (flags == BPF_EXIST) {
            map_unlock(map);
            return -5; /* Element does not exist */
        }

        if (map->cur_entries >= map->max_entries) {
            map_unlock(map);
            return -6; /* Map is full */
        }

        size_t total_sz = sizeof(struct cherry_ebpf_map_elem) + map->key_size + map->value_size;
        struct cherry_ebpf_map_elem *elem = (struct cherry_ebpf_map_elem *)kmalloc(total_sz, GFP_ATOMIC);
        if (!elem) {
            map_unlock(map);
            return -7;
        }

        elem->hash = hash;
        memcpy(elem->data, key, map->key_size);
        memcpy(elem->data + map->key_size, value, map->value_size);
        elem->next = map->buckets[bucket];
        map->buckets[bucket] = elem;
        map->cur_entries++;
        map_unlock(map);
        return 0;
    }

    return -1;
}

/*
 * Map Delete
 */
int cherry_ebpf_map_delete(struct bpf_map *map, const void *key)
{
    if (!map || !key) return -1;
    if (map->type == BPF_MAP_TYPE_ARRAY) return -2; /* Cannot delete array slots */

    if (map->type == BPF_MAP_TYPE_HASH) {
        uint32_t hash = fnv1a_hash(key, map->key_size);
        uint32_t bucket = hash % map->num_buckets;

        map_lock(map);
        struct cherry_ebpf_map_elem **prev = &map->buckets[bucket];
        while (*prev) {
            struct cherry_ebpf_map_elem *cur = *prev;
            if (cur->hash == hash && memcmp(cur->data, key, map->key_size) == 0) {
                *prev = cur->next;
                map->cur_entries--;
                map_unlock(map);
                kfree(cur);
                return 0;
            }
            prev = &cur->next;
        }
        map_unlock(map);
        return -3; /* Not found */
    }

    return -1;
}

int cherry_ebpf_map_get_next_key(struct bpf_map *map, const void *key, void *next_key)
{
    if (!map || !next_key) return -1;

    if (map->type == BPF_MAP_TYPE_ARRAY) {
        uint32_t next_idx = 0;
        if (key) {
            next_idx = *(const uint32_t *)key + 1;
        }
        if (next_idx >= map->max_entries) return -2;
        *(uint32_t *)next_key = next_idx;
        return 0;
    }

    if (map->type == BPF_MAP_TYPE_HASH) {
        map_lock(map);
        bool found_current = (key == NULL);
        for (uint32_t b = 0; b < map->num_buckets; b++) {
            struct cherry_ebpf_map_elem *cur = map->buckets[b];
            while (cur) {
                if (found_current) {
                    memcpy(next_key, cur->data, map->key_size);
                    map_unlock(map);
                    return 0;
                }
                if (key && memcmp(cur->data, key, map->key_size) == 0) {
                    found_current = true;
                }
                cur = cur->next;
            }
        }
        map_unlock(map);
        return -3; /* No more keys */
    }

    return -1;
}

/*
 * RingBuffer Operations
 */
long cherry_ebpf_ringbuf_output(struct bpf_map *map, const void *data, uint64_t size, uint64_t flags)
{
    (void)flags;
    if (!map || map->type != BPF_MAP_TYPE_RINGBUF || !data || size == 0) return -1;

    map_lock(map);
    uint32_t head = map->rb_head;
    uint32_t tail = map->rb_tail;
    uint32_t free_space = map->rb_size - (tail - head);
    uint32_t record_len = (uint32_t)(sizeof(struct cherry_ebpf_rb_hdr) + size);

    /* Align to 8 bytes */
    record_len = (record_len + 7) & ~7;

    if (free_space < record_len) {
        map_unlock(map);
        return -2; /* Ringbuffer full / dropped */
    }

    struct cherry_ebpf_rb_hdr hdr;
    hdr.len = (uint32_t)size;
    hdr.flags = 0;
    hdr.timestamp = 0;

    /* Write header */
    uint8_t *buf = (uint8_t *)map->storage;
    for (size_t i = 0; i < sizeof(hdr); i++) {
        buf[(tail + i) & map->rb_mask] = ((const uint8_t *)&hdr)[i];
    }
    tail += sizeof(hdr);

    /* Write data */
    for (size_t i = 0; i < size; i++) {
        buf[(tail + i) & map->rb_mask] = ((const uint8_t *)data)[i];
    }
    tail += (record_len - sizeof(hdr));

    map->rb_tail = tail;
    map_unlock(map);

    return 0;
}

int cherry_ebpf_ringbuf_read(struct bpf_map *map,
                              void (*cb)(const void *data, size_t size, void *user_ctx),
                              void *user_ctx)
{
    if (!map || map->type != BPF_MAP_TYPE_RINGBUF || !cb) return -1;

    map_lock(map);
    uint32_t head = map->rb_head;
    uint32_t tail = map->rb_tail;
    int events_read = 0;
    uint8_t *buf = (uint8_t *)map->storage;

    while (head != tail) {
        struct cherry_ebpf_rb_hdr hdr;
        for (size_t i = 0; i < sizeof(hdr); i++) {
            ((uint8_t *)&hdr)[i] = buf[(head + i) & map->rb_mask];
        }
        head += sizeof(hdr);

        uint8_t temp[256];
        uint32_t copy_len = hdr.len < sizeof(temp) ? hdr.len : sizeof(temp);
        for (size_t i = 0; i < copy_len; i++) {
            temp[i] = buf[(head + i) & map->rb_mask];
        }

        uint32_t record_len = (uint32_t)(sizeof(struct cherry_ebpf_rb_hdr) + hdr.len);
        record_len = (record_len + 7) & ~7;
        head += (record_len - sizeof(hdr));

        cb(temp, copy_len, user_ctx);
        events_read++;
    }

    map->rb_head = head;
    map_unlock(map);
    return events_read;
}

int cherry_ebpf_map_for_each(int (*cb)(struct bpf_map *map, void *arg), void *arg)
{
    if (!cb) return -1;
    map_global_lock();
    struct bpf_map *m = g_map_list;
    while (m) {
        int ret = cb(m, arg);
        if (ret != 0) {
            map_global_unlock();
            return ret;
        }
        m = m->next;
    }
    map_global_unlock();
    return 0;
}

uint32_t cherry_ebpf_map_count(void)
{
    uint32_t cnt = 0;
    map_global_lock();
    struct bpf_map *m = g_map_list;
    while (m) {
        cnt++;
        m = m->next;
    }
    map_global_unlock();
    return cnt;
}
