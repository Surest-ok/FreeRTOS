/*
 * CherryDRM - Direct Rendering Manager Subsystem
 *
 * Master Header for Kernel & Driver Integration
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_DRM_MASTER_H_
#define _CHERRY_DRM_MASTER_H_

#include <stdint.h>
#include <stdbool.h>
#include "drm/drm.h"
#include "drm/drm_mode.h"
#include "drm/virtgpu_drm.h"
#include "drm/panfrost_drm.h"
#include "cherry_kal.h"
#include "vfs_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DRM_MAX_DEVICES         4
#define DRM_MAX_GEM_OBJECTS     256
#define DRM_MAX_FENCE_ID        65535
#define DRM_MAX_PRIME_BUFFERS   64

struct drm_device;
struct drm_gem_object;
struct drm_crtc;
struct drm_connector;
struct drm_encoder;
struct drm_framebuffer;

/* DRM Driver Operations */
struct drm_driver {
    const char *name;
    const char *desc;
    const char *date;
    int         major;
    int         minor;
    int         patchlevel;
    uint32_t    driver_features;

    int (*load)(struct drm_device *dev);
    int (*unload)(struct drm_device *dev);
    int (*open)(struct drm_device *dev);
    int (*close)(struct drm_device *dev);
    int (*ioctl)(struct drm_device *dev, unsigned int cmd, void *arg);

    /* GEM Operations */
    struct drm_gem_object *(*gem_create)(struct drm_device *dev, size_t size, uint32_t flags);
    void (*gem_free)(struct drm_gem_object *obj);
    int (*dumb_create)(struct drm_device *dev, struct drm_mode_create_dumb *args);
    int (*dumb_map)(struct drm_device *dev, struct drm_mode_map_dumb *args);
    int (*dumb_destroy)(struct drm_device *dev, struct drm_mode_destroy_dumb *args);
};

#define DRIVER_MODESET          (1 << 0)
#define DRIVER_GEM              (1 << 1)
#define DRIVER_RENDER           (1 << 2)
#define DRIVER_ATOMIC           (1 << 3)
#define DRIVER_SYNCOBJ          (1 << 4)

/* GEM Object Representation */
struct drm_gem_object {
    uint32_t            handle;
    size_t              size;
    uint32_t            flags;
    uint32_t            refcount;
    void               *vaddr;
    uint64_t            gpu_addr;
    uint32_t            pitch;
    uint32_t            width;
    uint32_t            height;
    uint32_t            bpp;
    bool                is_dumb;
    int32_t             prime_fd;
    struct drm_device  *dev;
};

/* KMS Framebuffer */
struct drm_framebuffer {
    uint32_t            fb_id;
    uint32_t            width;
    uint32_t            height;
    uint32_t            pixel_format;
    uint32_t            pitches[4];
    uint32_t            offsets[4];
    struct drm_gem_object *objs[4];
    struct drm_device  *dev;
    bool                active;
};

/* KMS CRTC */
struct drm_crtc {
    uint32_t            crtc_id;
    uint32_t            fb_id;
    struct drm_framebuffer *fb;
    uint32_t            x;
    uint32_t            y;
    bool                enabled;
    struct drm_mode_modeinfo mode;
};

/* KMS Connector */
struct drm_connector {
    uint32_t            connector_id;
    uint32_t            connector_type;
    uint32_t            encoder_id;
    uint32_t            connection;
    uint32_t            count_modes;
    struct drm_mode_modeinfo modes[4];
    char                name[32];
};

/* KMS Encoder */
struct drm_encoder {
    uint32_t            encoder_id;
    uint32_t            encoder_type;
    uint32_t            crtc_id;
    uint32_t            possible_crtcs;
};

/* DRM Device Representation */
struct drm_device {
    uint32_t            index;
    const char         *name;
    struct drm_driver  *driver;
    void               *dev_private;
    bool                is_render_node;

    /* GEM Table */
    struct drm_gem_object *gem_objects[DRM_MAX_GEM_OBJECTS];
    uint32_t            gem_next_handle;
    struct mutex        gem_lock;

    /* KMS State */
    struct drm_crtc         crtc;
    struct drm_connector    connector;
    struct drm_encoder      encoder;
    struct drm_framebuffer  fbs[8];
    uint32_t                next_fb_id;

    /* PRIME DMA-BUF Table */
    struct drm_gem_object *prime_table[DRM_MAX_PRIME_BUFFERS];
    int                     next_prime_fd;

    /* VBlank / Page Flip */
    uint64_t            vblank_sequence;
    uint64_t            last_vblank_time_ns;
};

/* Subsystem Management APIs */
int cherry_drm_init(void);
int cherry_drm_run_tests(void);
struct drm_device *cherry_drm_get_primary(void);
void cherry_drm_set_primary(struct drm_device *dev);
struct drm_device *cherry_virtio_gpu_get_device(void);
int cherry_mali_bifrost_scanout_flush(struct drm_device *dev);
struct drm_device *cherry_drm_get_render(void);

/* Core IOCTL Engine */
int cherry_drm_ioctl(struct drm_device *dev, unsigned int cmd, void *arg);

/* GEM Helper Functions */
struct drm_gem_object *drm_gem_object_alloc(struct drm_device *dev, size_t size, uint32_t flags);
void drm_gem_object_free(struct drm_gem_object *obj);
int drm_gem_handle_create(struct drm_device *dev, struct drm_gem_object *obj, uint32_t *handle_out);
struct drm_gem_object *drm_gem_object_lookup(struct drm_device *dev, uint32_t handle);

/* Driver Entry Points */
int cherry_virtio_gpu_init(void);
int cherry_mali_panfrost_init(void);

/* Framebuffer Console (/dev/fb0) */
int cherry_fbdev_init(struct drm_device *dev);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_DRM_MASTER_H_ */
