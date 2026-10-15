/*
 * CherryEBPF - Shell Commands (ebpf / bpftool: list, maps, tp, top, dump)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "cherry_ebpf.h"
#include "cherry_shell.h"

extern int uart_printf(const char *fmt, ...);

static int print_prog_cb(struct cherry_ebpf_prog *p, void *arg)
{
    (void)arg;
    uart_printf("  [%u] %-16s type=%u insns=%u runs=%llu\n",
                p->id, p->name, p->type, p->insn_cnt, (unsigned long long)p->run_count);
    return 0;
}

static int print_map_cb(struct bpf_map *m, void *arg)
{
    (void)arg;
    const char *t_name = "unknown";
    if (m->type == BPF_MAP_TYPE_HASH) t_name = "hash";
    else if (m->type == BPF_MAP_TYPE_ARRAY) t_name = "array";
    else if (m->type == BPF_MAP_TYPE_RINGBUF) t_name = "ringbuf";

    uart_printf("  [%u] %-16s type=%-8s key_sz=%u val_sz=%u max=%u cur=%u\n",
                m->id, m->name, t_name, m->key_size, m->value_size,
                m->max_entries, m->cur_entries);
    return 0;
}

int cherry_ebpf_cmd_ebpf(int argc, char **argv)
{
    if (argc < 2 || strcmp(argv[1], "help") == 0) {
        uart_printf("Usage: ebpf <subcommand> [args]\n");
        uart_printf("Subcommands:\n");
        uart_printf("  list / prog     - List all loaded eBPF programs\n");
        uart_printf("  maps / map      - List all eBPF maps\n");
        uart_printf("  tp / tracepoint - List kernel tracepoints & stats\n");
        uart_printf("  top             - Show live kernel activity rates\n");
        uart_printf("  dump <map_id>   - Dump key-value elements in map\n");
        return 0;
    }

    const char *sub = argv[1];

    if (strcmp(sub, "list") == 0 || strcmp(sub, "prog") == 0) {
        uart_printf("=== Cherry eBPF Loaded Programs ===\n");
        cherry_ebpf_prog_for_each(print_prog_cb, NULL);
        return 0;
    }

    if (strcmp(sub, "maps") == 0 || strcmp(sub, "map") == 0) {
        uart_printf("=== Cherry eBPF Maps ===\n");
        cherry_ebpf_map_for_each(print_map_cb, NULL);
        return 0;
    }

    if (strcmp(sub, "tp") == 0 || strcmp(sub, "tracepoint") == 0) {
        uart_printf("=== Cherry Kernel Activity Tracepoints ===\n");
        for (uint32_t i = 0; i < CHERRY_TP_MAX; i++) {
            const struct cherry_tracepoint *tp = cherry_ebpf_get_tp(i);
            if (!tp) continue;
            uint32_t prog_cnt = 0;
            struct cherry_ebpf_prog_node *cur = tp->progs;
            while (cur) { prog_cnt++; cur = cur->next; }
            uart_printf("  [%2u] %-20s cat=%-8s hits=%-8llu progs=%u\n",
                        tp->id, tp->name, tp->category,
                        (unsigned long long)tp->hit_count, prog_cnt);
        }
        return 0;
    }

    if (strcmp(sub, "top") == 0) {
        struct cherry_ebpf_stats stats;
        cherry_ebpf_get_stats(&stats);
        uart_printf("=== Cherry Kernel Activity Monitor (eBPF Top) ===\n");
        uart_printf("  Total Trace Events : %llu\n", (unsigned long long)stats.total_events);
        uart_printf("  Program Executions : %llu\n", (unsigned long long)stats.total_prog_execs);
        uart_printf("  Active Programs    : %u\n", stats.active_programs);
        uart_printf("  Active Maps        : %u\n", stats.active_maps);
        uart_printf("Subsystem Hits:\n");
        for (uint32_t i = 0; i < CHERRY_TP_MAX; i++) {
            const struct cherry_tracepoint *tp = cherry_ebpf_get_tp(i);
            if (tp && tp->hit_count > 0) {
                uart_printf("  %-20s : %llu hits\n", tp->name, (unsigned long long)tp->hit_count);
            }
        }
        return 0;
    }

    if (strcmp(sub, "dump") == 0) {
        if (argc < 3) {
            uart_printf("Usage: ebpf dump <map_id>\n");
            return -1;
        }
        uint32_t map_id = (uint32_t)atoi(argv[2]);
        struct bpf_map *map = cherry_ebpf_map_find_by_id(map_id);
        if (!map) {
            uart_printf("ebpf: map [%u] not found\n", map_id);
            return -1;
        }
        uart_printf("=== Dump Map [%u] '%s' (%u entries) ===\n", map->id, map->name, map->cur_entries);
        if (map->type == BPF_MAP_TYPE_ARRAY) {
            for (uint32_t i = 0; i < map->max_entries; i++) {
                void *val = cherry_ebpf_map_lookup(map, &i);
                if (val) {
                    if (map->value_size == 4) {
                        uart_printf("  [%u] => %u (0x%x)\n", i, *(uint32_t *)val, *(uint32_t *)val);
                    } else if (map->value_size == 8) {
                        uart_printf("  [%u] => %llu (0x%llx)\n", i, (unsigned long long)*(uint64_t *)val, (unsigned long long)*(uint64_t *)val);
                    } else {
                        uart_printf("  [%u] => [val_sz=%u]\n", i, map->value_size);
                    }
                }
            }
        } else if (map->type == BPF_MAP_TYPE_HASH) {
            uint32_t key_buf[16] = {0};
            uint32_t next_buf[16] = {0};
            bool has_next = (cherry_ebpf_map_get_next_key(map, NULL, next_buf) == 0);
            while (has_next) {
                void *val = cherry_ebpf_map_lookup(map, next_buf);
                if (val) {
                    if (map->key_size == 4 && map->value_size == 8) {
                        uart_printf("  key=%u => val=%llu\n", *(uint32_t *)next_buf, (unsigned long long)*(uint64_t *)val);
                    } else if (map->key_size == 4 && map->value_size == 4) {
                        uart_printf("  key=%u => val=%u\n", *(uint32_t *)next_buf, *(uint32_t *)val);
                    } else {
                        uart_printf("  key_sz=%u => val_sz=%u\n", map->key_size, map->value_size);
                    }
                }
                memcpy(key_buf, next_buf, map->key_size);
                has_next = (cherry_ebpf_map_get_next_key(map, key_buf, next_buf) == 0);
            }
        }
        return 0;
    }

    uart_printf("ebpf: unknown subcommand '%s'\n", sub);
    return -1;
}

CHERRY_SHELL_EXPORT_CMD(ebpf, cherry_ebpf_cmd_ebpf, "eBPF subsystem manager: ebpf <list|maps|tp|top|dump>");
CHERRY_SHELL_EXPORT_CMD(bpftool, cherry_ebpf_cmd_ebpf, "alias to ebpf");
