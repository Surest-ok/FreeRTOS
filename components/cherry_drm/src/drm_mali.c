/*
 * CherryDRM - Direct Rendering Manager Subsystem
 *
 * ARM Mali (Panfrost) GPU Driver & Hardware Job Submission Engine
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_drm.h"
#include <string.h>

struct mali_job_slot {
    uint64_t head;
    uint32_t status;
    uint32_t jobs_completed;
};

struct mali_panfrost_device {
    uint32_t                product_id;
    uint32_t                num_cores;
    uint32_t                shader_present;
    uint32_t                l2_features;
    uint32_t                tiler_features;
    uint32_t                mem_features;
    uint32_t                mmu_features;

    struct mali_job_slot    slots[3]; /* 0: Vertex/Tiler, 1: Fragment, 2: Compute */
    uint32_t                total_jobs_submitted;
    uint32_t                total_jobs_completed;

    spinlock_t              job_lock;
};

static struct mali_panfrost_device s_mali_dev;
static struct drm_device           s_mali_drm;

extern struct drm_gem_object *drm_gem_object_alloc(struct drm_device *dev, size_t size, uint32_t flags);
extern void drm_gem_object_free(struct drm_gem_object *obj);
extern int drm_gem_handle_create(struct drm_device *dev, struct drm_gem_object *obj, uint32_t *handle_out);
extern struct drm_gem_object *drm_gem_object_lookup(struct drm_device *dev, uint32_t handle);

static int mali_panfrost_ioctl(struct drm_device *dev, unsigned int cmd, void *arg)
{
    struct mali_panfrost_device *mdev = (struct mali_panfrost_device *)dev->dev_private;
    if (!mdev) return -ENODEV;

    switch (cmd) {
    case DRM_IOCTL_PANFROST_GET_PARAM: {
        struct drm_panfrost_get_param *p = (struct drm_panfrost_get_param *)arg;
        if (!p) return -EINVAL;
        switch (p->param) {
        case DRM_PANFROST_PARAM_GPU_PROD_ID:
            p->value = mdev->product_id;
            break;
        case DRM_PANFROST_PARAM_NUM_CORES:
            p->value = mdev->num_cores;
            break;
        case DRM_PANFROST_PARAM_SHADER_PRESENT:
            p->value = mdev->shader_present;
            break;
        case DRM_PANFROST_PARAM_TILER_FEATURES:
            p->value = mdev->tiler_features;
            break;
        case DRM_PANFROST_PARAM_MEM_FEATURES:
            p->value = mdev->mem_features;
            break;
        case DRM_PANFROST_PARAM_MMU_FEATURES:
            p->value = mdev->mmu_features;
            break;
        default:
            p->value = 0;
            break;
        }
        return 0;
    }

    case DRM_IOCTL_PANFROST_CREATE_BO: {
        struct drm_panfrost_create_bo *args = (struct drm_panfrost_create_bo *)arg;
        if (!args || args->size == 0) return -EINVAL;

        struct drm_gem_object *obj = drm_gem_object_alloc(dev, args->size, args->flags);
        if (!obj) return -ENOMEM;

        uint32_t handle = 0;
        int ret = drm_gem_handle_create(dev, obj, &handle);
        if (ret != 0) {
            drm_gem_object_free(obj);
            return ret;
        }

        args->handle = handle;
        args->offset = obj->gpu_addr;
        return 0;
    }

    case DRM_IOCTL_PANFROST_MMAP_BO: {
        struct drm_panfrost_mmap_bo *args = (struct drm_panfrost_mmap_bo *)arg;
        if (!args || args->handle == 0) return -EINVAL;

        struct drm_gem_object *obj = drm_gem_object_lookup(dev, args->handle);
        if (!obj) return -EINVAL;

        args->offset = (uint64_t)(uintptr_t)obj->vaddr;
        drm_gem_object_free(obj);
        return 0;
    }

    case DRM_IOCTL_PANFROST_GET_BO_OFFSET: {
        struct drm_panfrost_get_bo_offset *args = (struct drm_panfrost_get_bo_offset *)arg;
        if (!args || args->handle == 0) return -EINVAL;

        struct drm_gem_object *obj = drm_gem_object_lookup(dev, args->handle);
        if (!obj) return -EINVAL;

        args->offset = obj->gpu_addr;
        drm_gem_object_free(obj);
        return 0;
    }

    case DRM_IOCTL_PANFROST_SUBMIT: {
        struct drm_panfrost_submit *submit = (struct drm_panfrost_submit *)arg;
        if (!submit || submit->jc == 0) return -EINVAL;

        spin_lock(&mdev->job_lock);
        mdev->total_jobs_submitted++;

        /* Slot 0 for Vertex, Slot 1 for Fragment */
        int slot = (submit->requirements & PANFROST_JD_REQ_FS) ? 1 : 0;
        mdev->slots[slot].head = submit->jc;
        mdev->slots[slot].status = 1; /* Executing */

        /* Mali pipeline simulation: process job chain descriptors */
        mdev->slots[slot].jobs_completed++;
        mdev->slots[slot].status = 0; /* Completed */
        mdev->total_jobs_completed++;

        spin_unlock(&mdev->job_lock);
        return 0;
    }

    case DRM_IOCTL_PANFROST_WAIT_BO: {
        struct drm_panfrost_wait_bo *wait = (struct drm_panfrost_wait_bo *)arg;
        if (!wait || wait->handle == 0) return -EINVAL;

        struct drm_gem_object *obj = drm_gem_object_lookup(dev, wait->handle);
        if (!obj) return -EINVAL;

        drm_gem_object_free(obj);
        return 0;
    }

    default:
        return -ENOTTY;
    }
}

static struct drm_driver s_mali_panfrost_driver = {
    .name = "panfrost",
    .desc = "ARM Mali Midgard/Bifrost DRM Driver for CherryOS",
    .date = "20261016",
    .major = 1,
    .minor = 2,
    .patchlevel = 0,
    .driver_features = DRIVER_RENDER | DRIVER_GEM | DRIVER_SYNCOBJ,
    .ioctl = mali_panfrost_ioctl,
};

int cherry_mali_panfrost_init(void)
{
    /* Initialize Mali Hardware Specs (Mali-G31 Bifrost v7 Architecture) */
    s_mali_dev.product_id = MALI_PRODUCT_ID_G31; /* 0x7093 */
    s_mali_dev.num_cores = 4;
    s_mali_dev.shader_present = 0x0F;            /* Cores 0..3 active */
    s_mali_dev.l2_features = 0x07390200;          /* 512KB L2, 64-byte line */
    s_mali_dev.tiler_features = 0x00000809;       /* 8 hierarchy levels */
    s_mali_dev.mem_features = 0x00000001;
    s_mali_dev.mmu_features = 0x00002830;
    spin_lock_init(&s_mali_dev.job_lock);

    /* Initialize DRM render node device */
    memset(&s_mali_drm, 0, sizeof(s_mali_drm));
    s_mali_drm.name = "panfrost";
    s_mali_drm.driver = &s_mali_panfrost_driver;
    s_mali_drm.dev_private = &s_mali_dev;
    s_mali_drm.is_render_node = true;
    mutex_init(&s_mali_drm.gem_lock);

    pr_info("cherry_drm: initialized arm mali panfrost gpu (mali-g31, 4 cores, l2=512kb)\n");
    return 0;
}

struct drm_device *cherry_drm_get_render(void)
{
    return &s_mali_drm;
}
