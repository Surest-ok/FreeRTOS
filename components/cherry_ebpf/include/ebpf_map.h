/*
 * CherryEBPF - eBPF Map Subsystem Definitions
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Provides Linux-standard eBPF maps:
 * - BPF_MAP_TYPE_HASH: Dynamic hash table with key-value lookup and concurrency protection
 * - BPF_MAP_TYPE_ARRAY: Fast O(1) indexed flat memory array
 * - BPF_MAP_TYPE_RINGBUF: Lockless high-throughput event ring buffer
 */

#ifndef _CHERRY_EBPF_MAP_H_
#define _CHERRY_EBPF_MAP_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Map Types */
#define BPF_MAP_TYPE_UNSPEC       0
#define BPF_MAP_TYPE_HASH         1
#define BPF_MAP_TYPE_ARRAY        2
#define BPF_MAP_TYPE_PROG_ARRAY   3
#define BPF_MAP_TYPE_PERF_EVENT_ARRAY 4
#define BPF_MAP_TYPE_PERCPU_HASH  5
#define BPF_MAP_TYPE_PERCPU_ARRAY 6
#define BPF_MAP_TYPE_RINGBUF      27

/* Map Update Flags */
#define BPF_ANY     0   /* Create new element or update existing */
#define BPF_NOEXIST 1   /* Create new element only if it did not exist */
#define BPF_EXIST   2   /* Update existing element only */

/* Hash Map Node */
struct cherry_ebpf_map_elem {
    struct cherry_ebpf_map_elem *next;
    uint32_t hash;
    uint8_t  data[]; /* key (key_size bytes) followed immediately by value (value_size bytes) */
};

/* RingBuffer Header */
struct cherry_ebpf_rb_hdr {
    uint32_t len;        /* Payload length */
    uint32_t flags;      /* Flags */
    uint64_t timestamp;  /* Timestamp in nanoseconds */
};

/* Core eBPF Map Descriptor */
struct bpf_map {
    uint32_t id;
    uint32_t type;
    uint32_t key_size;
    uint32_t value_size;
    uint32_t max_entries;
    uint32_t cur_entries;
    char     name[32];

    /* Hash table fields */
    struct cherry_ebpf_map_elem **buckets;
    uint32_t num_buckets;

    /* Array / RingBuf storage buffer */
    void    *storage;

    /* RingBuf specific fields */
    uint32_t rb_head;
    uint32_t rb_tail;
    uint32_t rb_mask;
    uint32_t rb_size;

    /* Multi-core spinlock */
    volatile uint32_t lock;

    struct bpf_map *next;
};

/* Map Management APIs */
struct bpf_map *cherry_ebpf_map_create(const char *name, uint32_t type,
                                       uint32_t key_size, uint32_t value_size,
                                       uint32_t max_entries);
void cherry_ebpf_map_destroy(struct bpf_map *map);
struct bpf_map *cherry_ebpf_map_find_by_id(uint32_t id);
struct bpf_map *cherry_ebpf_map_find_by_name(const char *name);

/* Data Manipulation APIs */
void *cherry_ebpf_map_lookup(struct bpf_map *map, const void *key);
int  cherry_ebpf_map_update(struct bpf_map *map, const void *key, const void *value, uint64_t flags);
int  cherry_ebpf_map_delete(struct bpf_map *map, const void *key);
int  cherry_ebpf_map_get_next_key(struct bpf_map *map, const void *key, void *next_key);

/* Ring Buffer APIs */
long cherry_ebpf_ringbuf_output(struct bpf_map *map, const void *data, uint64_t size, uint64_t flags);
void *cherry_ebpf_ringbuf_reserve(struct bpf_map *map, uint64_t size, uint64_t flags);
void cherry_ebpf_ringbuf_submit(void *data, uint64_t flags);
int  cherry_ebpf_ringbuf_read(struct bpf_map *map,
                              void (*cb)(const void *data, size_t size, void *user_ctx),
                              void *user_ctx);

/* Map Iteration for Tooling / Shell */
int  cherry_ebpf_map_for_each(int (*cb)(struct bpf_map *map, void *arg), void *arg);
uint32_t cherry_ebpf_map_count(void);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_EBPF_MAP_H_ */
