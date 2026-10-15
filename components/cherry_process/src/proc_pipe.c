/*
 * CherryPipe - In-Memory Virtual IPC Pipe Subsystem
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_pipe.h"
#include <string.h>

static ssize_t pipe_read(struct vfs_file *file, void *buf, size_t count)
{
    if (!file || !file->f_private || !buf || count == 0) {
        return -EINVAL;
    }

    struct cherry_pipe *pipe = (struct cherry_pipe *)file->f_private;
    uint8_t *dst = (uint8_t *)buf;
    size_t bytes_read = 0;

    for (;;) {
        mutex_lock(&pipe->lock);

        if (pipe->count > 0) {
            while (pipe->count > 0 && bytes_read < count) {
                dst[bytes_read++] = pipe->buffer[pipe->tail];
                pipe->tail = (pipe->tail + 1) % CHERRY_PIPE_BUF_SIZE;
                pipe->count--;
            }

            /* Notify any waiting writer that buffer space has freed up */
            up(&pipe->write_sem);
            mutex_unlock(&pipe->lock);
            return (ssize_t)bytes_read;
        }

        /* Pipe is empty */
        if (pipe->writers == 0) {
            /* All writers closed; EOF */
            mutex_unlock(&pipe->lock);
            return (ssize_t)bytes_read;
        }

        /* Non-blocking check */
        if (file->f_flags & O_NONBLOCK) {
            mutex_unlock(&pipe->lock);
            return -EAGAIN;
        }

        /* Wait for writer to produce data */
        mutex_unlock(&pipe->lock);
        down(&pipe->read_sem);
    }
}

static ssize_t pipe_write(struct vfs_file *file, const void *buf, size_t count)
{
    if (!file || !file->f_private || !buf || count == 0) {
        return -EINVAL;
    }

    struct cherry_pipe *pipe = (struct cherry_pipe *)file->f_private;
    const uint8_t *src = (const uint8_t *)buf;
    size_t bytes_written = 0;

    for (;;) {
        mutex_lock(&pipe->lock);

        /* Check broken pipe (no readers) */
        if (pipe->readers == 0) {
            mutex_unlock(&pipe->lock);
            return -EIO; /* Broken pipe */
        }

        while (pipe->count < CHERRY_PIPE_BUF_SIZE && bytes_written < count) {
            pipe->buffer[pipe->head] = src[bytes_written++];
            pipe->head = (pipe->head + 1) % CHERRY_PIPE_BUF_SIZE;
            pipe->count++;
        }

        if (bytes_written > 0) {
            /* Notify any waiting readers */
            up(&pipe->read_sem);
            mutex_unlock(&pipe->lock);
            return (ssize_t)bytes_written;
        }

        /* Buffer is full */
        if (file->f_flags & O_NONBLOCK) {
            mutex_unlock(&pipe->lock);
            return -EAGAIN;
        }

        /* Wait for reader to consume data */
        mutex_unlock(&pipe->lock);
        down(&pipe->write_sem);
    }
}

static int pipe_read_close(struct vfs_file *file)
{
    if (!file || !file->f_private) return VFS_OK;

    struct cherry_pipe *pipe = (struct cherry_pipe *)file->f_private;
    mutex_lock(&pipe->lock);
    pipe->readers--;
    /* Wake up any blocked writers */
    up(&pipe->write_sem);
    bool should_free = (pipe->readers <= 0 && pipe->writers <= 0);
    mutex_unlock(&pipe->lock);

    if (should_free) {
        mutex_destroy(&pipe->lock);
        sema_destroy(&pipe->read_sem);
        sema_destroy(&pipe->write_sem);
        kfree(pipe);
    }
    return VFS_OK;
}

static int pipe_write_close(struct vfs_file *file)
{
    if (!file || !file->f_private) return VFS_OK;

    struct cherry_pipe *pipe = (struct cherry_pipe *)file->f_private;
    mutex_lock(&pipe->lock);
    pipe->writers--;
    /* Wake up any blocked readers so they receive EOF */
    up(&pipe->read_sem);
    bool should_free = (pipe->readers <= 0 && pipe->writers <= 0);
    mutex_unlock(&pipe->lock);

    if (should_free) {
        mutex_destroy(&pipe->lock);
        sema_destroy(&pipe->read_sem);
        sema_destroy(&pipe->write_sem);
        kfree(pipe);
    }
    return VFS_OK;
}

static const struct vfs_file_operations g_pipe_read_fops = {
    .read  = pipe_read,
    .close = pipe_read_close,
};

static const struct vfs_file_operations g_pipe_write_fops = {
    .write = pipe_write,
    .close = pipe_write_close,
};

int cherry_pipe_create(int *out_read_vfs_fd, int *out_write_vfs_fd)
{
    if (!out_read_vfs_fd || !out_write_vfs_fd) {
        return -EINVAL;
    }

    struct cherry_pipe *pipe = (struct cherry_pipe *)kmalloc(sizeof(struct cherry_pipe), GFP_KERNEL);
    if (!pipe) {
        return -ENOMEM;
    }
    memset(pipe, 0, sizeof(struct cherry_pipe));

    mutex_init(&pipe->lock);
    sema_init(&pipe->read_sem, 0);
    sema_init(&pipe->write_sem, 0);
    pipe->readers = 1;
    pipe->writers = 1;

    /* Allocate Read File */
    struct vfs_file *rf = (struct vfs_file *)kmalloc(sizeof(struct vfs_file), GFP_KERNEL);
    if (!rf) {
        kfree(pipe);
        return -ENOMEM;
    }
    memset(rf, 0, sizeof(struct vfs_file));
    rf->f_ops = &g_pipe_read_fops;
    rf->f_flags = O_RDONLY;
    rf->f_private = pipe;
    rf->f_ref = 1;

    /* Allocate Write File */
    struct vfs_file *wf = (struct vfs_file *)kmalloc(sizeof(struct vfs_file), GFP_KERNEL);
    if (!wf) {
        kfree(rf);
        kfree(pipe);
        return -ENOMEM;
    }
    memset(wf, 0, sizeof(struct vfs_file));
    wf->f_ops = &g_pipe_write_fops;
    wf->f_flags = O_WRONLY;
    wf->f_private = pipe;
    wf->f_ref = 1;

    int rfd = vfs_fd_alloc(rf);
    if (rfd < 0) {
        kfree(rf);
        kfree(wf);
        kfree(pipe);
        return rfd;
    }

    int wfd = vfs_fd_alloc(wf);
    if (wfd < 0) {
        vfs_fd_free(rfd);
        kfree(rf);
        kfree(wf);
        kfree(pipe);
        return wfd;
    }

    *out_read_vfs_fd = rfd;
    *out_write_vfs_fd = wfd;
    return 0;
}
