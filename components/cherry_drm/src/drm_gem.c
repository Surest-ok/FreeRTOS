/*
 * CherryDRM - Direct Rendering Manager Subsystem
 *
 * GEM (Graphics Execution Manager) & PRIME Buffer Object Management
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_drm.h"
#include <string.h>

/* Global PRIME DMA-BUF Registry across all DRM devices */
static struct drm_gem_object *s_global_prime_table[DRM_MAX_PRIME_BUFFERS];
static kal_mutex_t            s_global_prime_lock;
static bool                   s_global_prime_inited = false;
static uint32_t               s_global_prime_next = 0;

static void prime_global_init_once(void)
{
    if (!s_global_prime_inited) {
        mutex_init(&s_global_prime_lock);
        s_global_prime_inited = true;
    }
}

struct drm_gem_object *drm_gem_object_alloc(struct drm_device *dev, size_t size, uint32_t flags)
{
    if (!dev || size == 0) return NULL;

    struct drm_gem_object *obj = (struct drm_gem_object *)kzalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj) return NULL;

    /* Align size to 4KB page boundary */
    size = (size + 4095) & ~4095UL;
    obj->size = size;
    obj->flags = flags;
    obj->refcount = 1;
    obj->dev = dev;
    obj->prime_fd = -1;

    /* Allocate backing physical/virtual memory */
    obj->vaddr = kzalloc(size, GFP_KERNEL);
    if (!obj->vaddr) {
        kfree(obj);
        return NULL;
    }
    obj->gpu_addr = (uint64_t)(uintptr_t)obj->vaddr;

    return obj;
}

void drm_gem_object_free(struct drm_gem_object *obj)
{
    if (!obj) return;
    if (--obj->refcount > 0) return;

    prime_global_init_once();
    mutex_lock(&s_global_prime_lock);
    if (obj->prime_fd >= 100 && obj->prime_fd < (100 + DRM_MAX_PRIME_BUFFERS)) {
        s_global_prime_table[obj->prime_fd - 100] = NULL;
    }
    mutex_unlock(&s_global_prime_lock);

    if (obj->vaddr) {
        kfree(obj->vaddr);
        obj->vaddr = NULL;
    }
    kfree(obj);
}

int drm_gem_handle_create(struct drm_device *dev, struct drm_gem_object *obj, uint32_t *handle_out)
{
    if (!dev || !obj || !handle_out) return -EINVAL;

    mutex_lock(&dev->gem_lock);
    for (int i = 1; i < DRM_MAX_GEM_OBJECTS; i++) {
        uint32_t h = (dev->gem_next_handle++) % DRM_MAX_GEM_OBJECTS;
        if (h == 0) h = 1;
        if (!dev->gem_objects[h]) {
            dev->gem_objects[h] = obj;
            obj->handle = h;
            *handle_out = h;
            mutex_unlock(&dev->gem_lock);
            return 0;
        }
    }
    mutex_unlock(&dev->gem_lock);
    return -ENOSPC;
}

struct drm_gem_object *drm_gem_object_lookup(struct drm_device *dev, uint32_t handle)
{
    if (!dev || handle == 0 || handle >= DRM_MAX_GEM_OBJECTS) return NULL;

    mutex_lock(&dev->gem_lock);
    struct drm_gem_object *obj = dev->gem_objects[handle];
    if (obj) {
        obj->refcount++;
    }
    mutex_unlock(&dev->gem_lock);
    return obj;
}

int drm_gem_close_ioctl(struct drm_device *dev, struct drm_gem_close *args)
{
    if (!dev || !args || args->handle == 0 || args->handle >= DRM_MAX_GEM_OBJECTS) {
        return -EINVAL;
    }

    mutex_lock(&dev->gem_lock);
    struct drm_gem_object *obj = dev->gem_objects[args->handle];
    if (!obj) {
        mutex_unlock(&dev->gem_lock);
        return -EINVAL;
    }
    dev->gem_objects[args->handle] = NULL;
    mutex_unlock(&dev->gem_lock);

    drm_gem_object_free(obj);
    return 0;
}

/* =========================================================================
 * KMS Dumb Buffer Allocations
 * ========================================================================= */
int drm_mode_create_dumb_ioctl(struct drm_device *dev, struct drm_mode_create_dumb *args)
{
    if (!dev || !args || args->width == 0 || args->height == 0) return -EINVAL;

    uint32_t bpp = args->bpp ? args->bpp : 32;
    uint32_t pitch = (args->width * ((bpp + 7) / 8) + 63) & ~63U; /* 64-byte alignment */
    size_t size = (size_t)pitch * args->height;

    struct drm_gem_object *obj = drm_gem_object_alloc(dev, size, 0);
    if (!obj) return -ENOMEM;

    obj->width = args->width;
    obj->height = args->height;
    obj->pitch = pitch;
    obj->bpp = bpp;
    obj->is_dumb = true;

    uint32_t handle = 0;
    int ret = drm_gem_handle_create(dev, obj, &handle);
    if (ret != 0) {
        drm_gem_object_free(obj);
        return ret;
    }

    args->handle = handle;
    args->pitch = pitch;
    args->size = size;
    return 0;
}

int drm_mode_map_dumb_ioctl(struct drm_device *dev, struct drm_mode_map_dumb *args)
{
    if (!dev || !args) return -EINVAL;

    struct drm_gem_object *obj = drm_gem_object_lookup(dev, args->handle);
    if (!obj) return -EINVAL;

    /* Fake mmap offset equal to virtual address or handle shifted */
    args->offset = (uint64_t)(uintptr_t)obj->vaddr;
    drm_gem_object_free(obj);
    return 0;
}

int drm_mode_destroy_dumb_ioctl(struct drm_device *dev, struct drm_mode_destroy_dumb *args)
{
    if (!dev || !args) return -EINVAL;
    struct drm_gem_close close_args;
    close_args.handle = args->handle;
    close_args.pad = 0;
    return drm_gem_close_ioctl(dev, &close_args);
}

/* =========================================================================
 * PRIME DMA-BUF Buffer Sharing (Cross-GPU Zero-Copy between VirtIO & Mali)
 * ========================================================================= */
int drm_prime_handle_to_fd_ioctl(struct drm_device *dev, struct drm_prime_handle *args)
{
    if (!dev || !args || args->handle == 0 || args->handle >= DRM_MAX_GEM_OBJECTS) return -EINVAL;
    prime_global_init_once();

    mutex_lock(&dev->gem_lock);
    struct drm_gem_object *obj = dev->gem_objects[args->handle];
    if (!obj) {
        mutex_unlock(&dev->gem_lock);
        return -EINVAL;
    }
    mutex_unlock(&dev->gem_lock);

    mutex_lock(&s_global_prime_lock);
    for (int i = 0; i < DRM_MAX_PRIME_BUFFERS; i++) {
        int fd = (s_global_prime_next++) % DRM_MAX_PRIME_BUFFERS;
        if (!s_global_prime_table[fd]) {
            s_global_prime_table[fd] = obj;
            obj->prime_fd = fd + 100;
            args->fd = obj->prime_fd;
            mutex_unlock(&s_global_prime_lock);
            return 0;
        }
    }
    mutex_unlock(&s_global_prime_lock);
    return -EMFILE;
}

int drm_prime_fd_to_handle_ioctl(struct drm_device *dev, struct drm_prime_handle *args)
{
    if (!dev || !args || args->fd < 100 || args->fd >= (100 + DRM_MAX_PRIME_BUFFERS)) {
        return -EBADF;
    }
    prime_global_init_once();

    int idx = args->fd - 100;
    mutex_lock(&s_global_prime_lock);
    struct drm_gem_object *obj = s_global_prime_table[idx];
    if (!obj) {
        mutex_unlock(&s_global_prime_lock);
        return -EBADF;
    }
    obj->refcount++;
    mutex_unlock(&s_global_prime_lock);

    /* Import buffer object into this device's GEM table */
    uint32_t handle = 0;
    int ret = drm_gem_handle_create(dev, obj, &handle);
    if (ret == 0) {
        args->handle = handle;
    } else {
        drm_gem_object_free(obj);
    }
    return ret;
}
