/*
 * CherryMesa - Generic Buffer Management (GBM) Library
 *
 * Implements GBM Device, Surface & Buffer Objects over CherryDRM
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "gbm.h"
#include "cherry_kal.h"
#include "drm/drm.h"
#include "drm/drm_mode.h"
#include "drm/panfrost_drm.h"
#include "cherry_drm.h"
#include <string.h>

struct gbm_device {
    int                 drm_fd;
    struct drm_device  *drm_dev;
    bool                is_render;
};

struct gbm_bo {
    struct gbm_device  *device;
    uint32_t            width;
    uint32_t            height;
    uint32_t            stride;
    uint32_t            format;
    uint32_t            handle;
    size_t              size;
    void               *vaddr;
    int                 prime_fd;
};

struct gbm_surface {
    struct gbm_device  *device;
    uint32_t            width;
    uint32_t            height;
    uint32_t            format;
    uint32_t            flags;
    struct gbm_bo      *buffers[2]; /* Double buffering: front & back */
    int                 front_idx;
};

struct gbm_device *gbm_create_device(int fd)
{
    struct gbm_device *gbm = (struct gbm_device *)kzalloc(sizeof(*gbm), GFP_KERNEL);
    if (!gbm) return NULL;

    gbm->drm_fd = fd;
    /* Determine if fd refers to primary or render node */
    gbm->drm_dev = cherry_drm_get_primary();
    if (!gbm->drm_dev) {
        gbm->drm_dev = cherry_drm_get_render();
        gbm->is_render = true;
    }

    return gbm;
}

void gbm_device_destroy(struct gbm_device *gbm)
{
    if (!gbm) return;
    kfree(gbm);
}

int gbm_device_get_fd(struct gbm_device *gbm)
{
    return gbm ? gbm->drm_fd : -1;
}

struct gbm_bo *gbm_bo_create(struct gbm_device *gbm,
                             uint32_t width, uint32_t height,
                             uint32_t format, uint32_t flags)
{
    (void)flags;
    if (!gbm || !gbm->drm_dev || width == 0 || height == 0) return NULL;

    struct gbm_bo *bo = (struct gbm_bo *)kzalloc(sizeof(*bo), GFP_KERNEL);
    if (!bo) return NULL;

    bo->device = gbm;
    bo->width = width;
    bo->height = height;
    bo->format = format;
    bo->prime_fd = -1;

    /* Allocate buffer through CherryDRM IOCTL */
    if (gbm->is_render) {
        /* Allocate on Mali Panfrost GPU */
        struct drm_panfrost_create_bo c_bo;
        memset(&c_bo, 0, sizeof(c_bo));
        c_bo.size = width * height * 4;
        c_bo.flags = 0;
        if (cherry_drm_ioctl(gbm->drm_dev, DRM_IOCTL_PANFROST_CREATE_BO, &c_bo) == 0) {
            bo->handle = c_bo.handle;
            bo->size = c_bo.size;
            bo->stride = width * 4;

            struct drm_panfrost_mmap_bo m_bo;
            m_bo.handle = bo->handle;
            if (cherry_drm_ioctl(gbm->drm_dev, DRM_IOCTL_PANFROST_MMAP_BO, &m_bo) == 0) {
                bo->vaddr = (void *)(uintptr_t)m_bo.offset;
            }
        }
    } else {
        /* Allocate Dumb buffer on Primary VirtIO-GPU node */
        struct drm_mode_create_dumb c_dumb;
        memset(&c_dumb, 0, sizeof(c_dumb));
        c_dumb.width = width;
        c_dumb.height = height;
        c_dumb.bpp = 32;

        if (cherry_drm_ioctl(gbm->drm_dev, DRM_IOCTL_MODE_CREATE_DUMB, &c_dumb) == 0) {
            bo->handle = c_dumb.handle;
            bo->size = c_dumb.size;
            bo->stride = c_dumb.pitch;

            struct drm_mode_map_dumb m_dumb;
            m_dumb.handle = bo->handle;
            if (cherry_drm_ioctl(gbm->drm_dev, DRM_IOCTL_MODE_MAP_DUMB, &m_dumb) == 0) {
                bo->vaddr = (void *)(uintptr_t)m_dumb.offset;
            }
        }
    }

    if (bo->handle == 0 || !bo->vaddr) {
        kfree(bo);
        return NULL;
    }

    return bo;
}

void gbm_bo_destroy(struct gbm_bo *bo)
{
    if (!bo || !bo->device || !bo->device->drm_dev) return;

    struct drm_gem_close gc;
    gc.handle = bo->handle;
    gc.pad = 0;
    cherry_drm_ioctl(bo->device->drm_dev, DRM_IOCTL_GEM_CLOSE, &gc);

    kfree(bo);
}

uint32_t gbm_bo_get_width(struct gbm_bo *bo)
{
    return bo ? bo->width : 0;
}

uint32_t gbm_bo_get_height(struct gbm_bo *bo)
{
    return bo ? bo->height : 0;
}

uint32_t gbm_bo_get_stride(struct gbm_bo *bo)
{
    return bo ? bo->stride : 0;
}

uint32_t gbm_bo_get_format(struct gbm_bo *bo)
{
    return bo ? bo->format : 0;
}

struct gbm_device *gbm_bo_get_device(struct gbm_bo *bo)
{
    return bo ? bo->device : NULL;
}

union gbm_bo_handle gbm_bo_get_handle(struct gbm_bo *bo)
{
    union gbm_bo_handle h;
    h.u32 = bo ? bo->handle : 0;
    return h;
}

int gbm_bo_get_fd(struct gbm_bo *bo)
{
    if (!bo || !bo->device || !bo->device->drm_dev) return -1;
    if (bo->prime_fd > 0) return bo->prime_fd;

    struct drm_prime_handle prime;
    memset(&prime, 0, sizeof(prime));
    prime.handle = bo->handle;
    prime.flags = DRM_CLOEXEC | DRM_RDWR;
    if (cherry_drm_ioctl(bo->device->drm_dev, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime) == 0) {
        bo->prime_fd = prime.fd;
        return bo->prime_fd;
    }
    return -1;
}

void *gbm_bo_map(struct gbm_bo *bo,
                 uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                 uint32_t flags, uint32_t *stride, void **map_data)
{
    (void)x; (void)y; (void)width; (void)height; (void)flags;
    if (!bo) return NULL;
    if (stride) *stride = bo->stride;
    if (map_data) *map_data = bo->vaddr;
    return bo->vaddr;
}

void gbm_bo_unmap(struct gbm_bo *bo, void *map_data)
{
    (void)bo; (void)map_data;
}

/* =========================================================================
 * GBM Surface Management (Double-Buffered EGL Scanout Support)
 * ========================================================================= */
struct gbm_surface *gbm_surface_create(struct gbm_device *gbm,
                                       uint32_t width, uint32_t height,
                                       uint32_t format, uint32_t flags)
{
    if (!gbm || width == 0 || height == 0) return NULL;

    struct gbm_surface *surf = (struct gbm_surface *)kzalloc(sizeof(*surf), GFP_KERNEL);
    if (!surf) return NULL;

    surf->device = gbm;
    surf->width = width;
    surf->height = height;
    surf->format = format;
    surf->flags = flags;
    surf->front_idx = 0;

    /* Allocate Front and Back buffers */
    surf->buffers[0] = gbm_bo_create(gbm, width, height, format, flags);
    surf->buffers[1] = gbm_bo_create(gbm, width, height, format, flags);

    if (!surf->buffers[0] || !surf->buffers[1]) {
        if (surf->buffers[0]) gbm_bo_destroy(surf->buffers[0]);
        if (surf->buffers[1]) gbm_bo_destroy(surf->buffers[1]);
        kfree(surf);
        return NULL;
    }

    return surf;
}

void gbm_surface_destroy(struct gbm_surface *surf)
{
    if (!surf) return;
    if (surf->buffers[0]) gbm_bo_destroy(surf->buffers[0]);
    if (surf->buffers[1]) gbm_bo_destroy(surf->buffers[1]);
    kfree(surf);
}

uint32_t gbm_surface_get_width(struct gbm_surface *surf)
{
    return surf ? surf->width : 0;
}

uint32_t gbm_surface_get_height(struct gbm_surface *surf)
{
    return surf ? surf->height : 0;
}

struct gbm_bo *gbm_surface_lock_front_buffer(struct gbm_surface *surf)
{
    if (!surf) return NULL;
    struct gbm_bo *bo = surf->buffers[surf->front_idx];
    surf->front_idx = 1 - surf->front_idx; /* Swap buffers */
    return bo;
}

void gbm_surface_release_buffer(struct gbm_surface *surf, struct gbm_bo *bo)
{
    (void)surf; (void)bo;
}

int gbm_surface_has_free_buffers(struct gbm_surface *surf)
{
    return surf ? 1 : 0;
}
