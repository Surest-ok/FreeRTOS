/*
 * CherryPipe - In-Memory Virtual IPC Pipe Subsystem
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Circular-buffer based unidirectional IPC channel with VFS integration.
 */

#ifndef _CHERRY_PIPE_H_
#define _CHERRY_PIPE_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "cherry_kal.h"
#include "cherry_vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CHERRY_PIPE_BUF_SIZE    4096

struct cherry_pipe {
    uint8_t             buffer[CHERRY_PIPE_BUF_SIZE];
    size_t              head;           /* Write index */
    size_t              tail;           /* Read index */
    size_t              count;          /* Bytes in pipe */
    int                 readers;        /* Active reader count */
    int                 writers;        /* Active writer count */
    struct mutex        lock;           /* Mutex protection */
    struct semaphore    read_sem;       /* Woken when data arrives */
    struct semaphore    write_sem;      /* Woken when space becomes available */
};

/**
 * Create a new IPC pipe pair returning two global VFS file descriptors.
 *
 * @param out_read_vfs_fd  Returns global VFS fd for reading
 * @param out_write_vfs_fd Returns global VFS fd for writing
 * @return 0 on success, negative error code on failure
 */
int cherry_pipe_create(int *out_read_vfs_fd, int *out_write_vfs_fd);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_PIPE_H_ */
