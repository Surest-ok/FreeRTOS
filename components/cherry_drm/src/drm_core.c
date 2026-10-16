/*
 * CherryDRM - Direct Rendering Manager Subsystem
 *
 * Core DRM Subsystem, IOCTL Dispatcher & Device Node Registration
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_drm.h"
#include "vfs_dev.h"
#include "vfs_posix.h"
#include <string.h>

extern int drm_gem_close_ioctl(struct drm_device *dev, struct drm_gem_close *args);
extern int drm_prime_handle_to_fd_ioctl(struct drm_device *dev, struct drm_prime_handle *args);
extern int drm_prime_fd_to_handle_ioctl(struct drm_device *dev, struct drm_prime_handle *args);

extern int drm_mode_getresources_ioctl(struct drm_device *dev, struct drm_mode_card_res *res);
extern int drm_mode_getcrtc_ioctl(struct drm_device *dev, struct drm_mode_crtc *crtc_req);
extern int drm_mode_setcrtc_ioctl(struct drm_device *dev, struct drm_mode_crtc *crtc_req);
extern int drm_mode_getconnector_ioctl(struct drm_device *dev, struct drm_mode_get_connector *conn_req);
extern int drm_mode_getencoder_ioctl(struct drm_device *dev, struct drm_mode_get_encoder *enc_req);
extern int drm_mode_addfb_ioctl(struct drm_device *dev, struct drm_mode_fb_cmd *cmd);
extern int drm_mode_addfb2_ioctl(struct drm_device *dev, struct drm_mode_fb_cmd2 *cmd2);
extern int drm_mode_rmfb_ioctl(struct drm_device *dev, uint32_t fb_id);
extern int drm_mode_page_flip_ioctl(struct drm_device *dev, struct drm_mode_crtc_page_flip *page_flip);
extern int drm_mode_dirtyfb_ioctl(struct drm_device *dev, struct drm_mode_fb_dirty_cmd *cmd);
extern int drm_mode_create_dumb_ioctl(struct drm_device *dev, struct drm_mode_create_dumb *args);
extern int drm_mode_map_dumb_ioctl(struct drm_device *dev, struct drm_mode_map_dumb *args);
extern int drm_mode_destroy_dumb_ioctl(struct drm_device *dev, struct drm_mode_destroy_dumb *args);

static struct vfs_cdev s_card0_cdev;
static struct vfs_cdev s_render_cdev;

/* Master IOCTL Dispatcher */
int cherry_drm_ioctl(struct drm_device *dev, unsigned int cmd, void *arg)
{
    if (!dev) return -ENODEV;

    switch (cmd) {
    case DRM_IOCTL_VERSION: {
        struct drm_version *v = (struct drm_version *)arg;
        if (!v) return -EINVAL;
        v->version_major = dev->driver ? dev->driver->major : 1;
        v->version_minor = dev->driver ? dev->driver->minor : 0;
        v->version_patchlevel = dev->driver ? dev->driver->patchlevel : 0;
        if (v->name && dev->driver && dev->driver->name) {
            strncpy(v->name, dev->driver->name, v->name_len - 1);
        }
        if (v->date && dev->driver && dev->driver->date) {
            strncpy(v->date, dev->driver->date, v->date_len - 1);
        }
        if (v->desc && dev->driver && dev->driver->desc) {
            strncpy(v->desc, dev->driver->desc, v->desc_len - 1);
        }
        return 0;
    }

    case DRM_IOCTL_GET_CAP: {
        struct drm_get_cap *cap = (struct drm_get_cap *)arg;
        if (!cap) return -EINVAL;
        switch (cap->capability) {
        case DRM_CAP_DUMB_BUFFER:
            cap->value = (dev->driver && (dev->driver->driver_features & DRIVER_MODESET)) ? 1 : 0;
            break;
        case DRM_CAP_PRIME:
            cap->value = DRM_PRIME_CAP_IMPORT | DRM_PRIME_CAP_EXPORT;
            break;
        case DRM_CAP_TIMESTAMP_MONOTONIC:
            cap->value = 1;
            break;
        case DRM_CAP_SYNCOBJ:
            cap->value = 1;
            break;
        default:
            cap->value = 0;
            break;
        }
        return 0;
    }

    case DRM_IOCTL_SET_CLIENT_CAP:
        return 0; /* Acknowledge universal planes & atomic capabilities */

    case DRM_IOCTL_GEM_CLOSE:
        return drm_gem_close_ioctl(dev, (struct drm_gem_close *)arg);

    case DRM_IOCTL_PRIME_HANDLE_TO_FD:
        return drm_prime_handle_to_fd_ioctl(dev, (struct drm_prime_handle *)arg);

    case DRM_IOCTL_PRIME_FD_TO_HANDLE:
        return drm_prime_fd_to_handle_ioctl(dev, (struct drm_prime_handle *)arg);

    case DRM_IOCTL_SYNCOBJ_CREATE: {
        struct drm_syncobj_create *so = (struct drm_syncobj_create *)arg;
        if (!so) return -EINVAL;
        so->handle = 1; /* virtual syncobj */
        return 0;
    }
    case DRM_IOCTL_SYNCOBJ_DESTROY:
    case DRM_IOCTL_SYNCOBJ_WAIT:
        return 0;

    /* KMS IOCTLs */
    case DRM_IOCTL_MODE_GETRESOURCES:
        return drm_mode_getresources_ioctl(dev, (struct drm_mode_card_res *)arg);

    case DRM_IOCTL_MODE_GETCRTC:
        return drm_mode_getcrtc_ioctl(dev, (struct drm_mode_crtc *)arg);

    case DRM_IOCTL_MODE_SETCRTC:
        return drm_mode_setcrtc_ioctl(dev, (struct drm_mode_crtc *)arg);

    case DRM_IOCTL_MODE_GETCONNECTOR:
        return drm_mode_getconnector_ioctl(dev, (struct drm_mode_get_connector *)arg);

    case DRM_IOCTL_MODE_GETENCODER:
        return drm_mode_getencoder_ioctl(dev, (struct drm_mode_get_encoder *)arg);

    case DRM_IOCTL_MODE_ADDFB:
        return drm_mode_addfb_ioctl(dev, (struct drm_mode_fb_cmd *)arg);

    case DRM_IOCTL_MODE_ADDFB2:
        return drm_mode_addfb2_ioctl(dev, (struct drm_mode_fb_cmd2 *)arg);

    case DRM_IOCTL_MODE_RMFB:
        return drm_mode_rmfb_ioctl(dev, *(uint32_t *)arg);

    case DRM_IOCTL_MODE_PAGE_FLIP:
        return drm_mode_page_flip_ioctl(dev, (struct drm_mode_crtc_page_flip *)arg);

    case DRM_IOCTL_MODE_DIRTYFB:
        return drm_mode_dirtyfb_ioctl(dev, (struct drm_mode_fb_dirty_cmd *)arg);

    case DRM_IOCTL_MODE_CREATE_DUMB:
        return drm_mode_create_dumb_ioctl(dev, (struct drm_mode_create_dumb *)arg);

    case DRM_IOCTL_MODE_MAP_DUMB:
        return drm_mode_map_dumb_ioctl(dev, (struct drm_mode_map_dumb *)arg);

    case DRM_IOCTL_MODE_DESTROY_DUMB:
        return drm_mode_destroy_dumb_ioctl(dev, (struct drm_mode_destroy_dumb *)arg);

    case DRM_IOCTL_MODE_GETPLANERESOURCES: {
        struct drm_mode_get_plane_res *pr = (struct drm_mode_get_plane_res *)arg;
        if (!pr) return -EINVAL;
        pr->count_planes = 0;
        return 0;
    }

    default:
        /* Delegate to device-specific driver IOCTL handler */
        if (dev->driver && dev->driver->ioctl) {
            return dev->driver->ioctl(dev, cmd, arg);
        }
        return -ENOTTY;
    }
}

/* VFS Character Device Wrapper for /dev/dri/card0 & /dev/card0 */
static int drm_cdev_open(struct vfs_cdev *cdev, int flags)
{
    (void)cdev;
    (void)flags;
    return 0;
}

static int drm_cdev_close(struct vfs_cdev *cdev)
{
    (void)cdev;
    return 0;
}

static int drm_card0_ioctl(struct vfs_cdev *cdev, unsigned int cmd, unsigned long arg)
{
    (void)cdev;
    struct drm_device *dev = cherry_drm_get_primary();
    return cherry_drm_ioctl(dev, cmd, (void *)arg);
}

static int drm_render_ioctl(struct vfs_cdev *cdev, unsigned int cmd, unsigned long arg)
{
    (void)cdev;
    struct drm_device *dev = cherry_drm_get_render();
    return cherry_drm_ioctl(dev, cmd, (void *)arg);
}

static const struct vfs_cdev_ops s_card0_ops = {
    .open = drm_cdev_open,
    .close = drm_cdev_close,
    .read = NULL,
    .write = NULL,
    .ioctl = drm_card0_ioctl,
    .poll = NULL,
};

static const struct vfs_cdev_ops s_render_ops = {
    .open = drm_cdev_open,
    .close = drm_cdev_close,
    .read = NULL,
    .write = NULL,
    .ioctl = drm_render_ioctl,
    .poll = NULL,
};

int cherry_drm_init(void)
{
    static bool s_inited = false;
    if (s_inited) return 0;

    pr_info("cherry_drm: initializing direct rendering manager (DRM) subsystem...\n");

    /* 1. Initialize VirtIO-GPU Driver (Primary Display & KMS) */
    cherry_virtio_gpu_init();

    /* 2. Initialize ARM Mali (Panfrost) Driver (Render Node) */
    cherry_mali_panfrost_init();

    /* 3. Register VFS character devices */
    memset(&s_card0_cdev, 0, sizeof(s_card0_cdev));
    s_card0_cdev.ops = &s_card0_ops;
    vfs_cdev_register(&s_card0_cdev, "card0");

    memset(&s_render_cdev, 0, sizeof(s_render_cdev));
    s_render_cdev.ops = &s_render_ops;
    vfs_cdev_register(&s_render_cdev, "renderD128");

    s_inited = true;
    pr_info("cherry_drm: registered /dev/card0 and /dev/renderD128 successfully\n");
    return 0;
}
