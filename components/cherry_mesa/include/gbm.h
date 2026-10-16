/*
 * CherryMesa - Generic Buffer Management (GBM) Library
 *
 * Upstream Mesa GBM UAPI
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_GBM_H_
#define _CHERRY_GBM_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Format FourCC codes */
#define GBM_FORMAT_RGB565       0x36314752
#define GBM_FORMAT_XRGB8888     0x34325258
#define GBM_FORMAT_ARGB8888     0x34325241

/* Buffer Usage Flags */
#define GBM_BO_USE_SCANOUT      (1 << 0)
#define GBM_BO_USE_CURSOR       (1 << 1)
#define GBM_BO_USE_RENDERING    (1 << 2)
#define GBM_BO_USE_WRITE        (1 << 3)
#define GBM_BO_USE_LINEAR       (1 << 4)

union gbm_bo_handle {
    void *ptr;
    int32_t s32;
    uint32_t u32;
    int64_t s64;
    uint64_t u64;
};

struct gbm_device;
struct gbm_bo;
struct gbm_surface;

/* Device Management */
struct gbm_device *gbm_create_device(int fd);
void gbm_device_destroy(struct gbm_device *gbm);
int gbm_device_get_fd(struct gbm_device *gbm);

/* Buffer Object Management */
struct gbm_bo *gbm_bo_create(struct gbm_device *gbm,
                             uint32_t width, uint32_t height,
                             uint32_t format, uint32_t flags);
void gbm_bo_destroy(struct gbm_bo *bo);

uint32_t gbm_bo_get_width(struct gbm_bo *bo);
uint32_t gbm_bo_get_height(struct gbm_bo *bo);
uint32_t gbm_bo_get_stride(struct gbm_bo *bo);
uint32_t gbm_bo_get_format(struct gbm_bo *bo);
struct gbm_device *gbm_bo_get_device(struct gbm_bo *bo);
union gbm_bo_handle gbm_bo_get_handle(struct gbm_bo *bo);
int gbm_bo_get_fd(struct gbm_bo *bo);

void *gbm_bo_map(struct gbm_bo *bo,
                 uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                 uint32_t flags, uint32_t *stride, void **map_data);
void gbm_bo_unmap(struct gbm_bo *bo, void *map_data);

/* Surface Management */
struct gbm_surface *gbm_surface_create(struct gbm_device *gbm,
                                       uint32_t width, uint32_t height,
                                       uint32_t format, uint32_t flags);
void gbm_surface_destroy(struct gbm_surface *surf);
uint32_t gbm_surface_get_width(struct gbm_surface *surf);
uint32_t gbm_surface_get_height(struct gbm_surface *surf);

struct gbm_bo *gbm_surface_lock_front_buffer(struct gbm_surface *surf);
void gbm_surface_release_buffer(struct gbm_surface *surf, struct gbm_bo *bo);
int gbm_surface_has_free_buffers(struct gbm_surface *surf);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_GBM_H_ */
