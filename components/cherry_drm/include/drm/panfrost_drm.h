/*
 * CherryDRM - Direct Rendering Manager for CherryOS / FreeRTOS
 *
 * ARM Mali / Panfrost DRM Driver UAPI & Architecture Definitions
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_PANFROST_DRM_H_
#define _CHERRY_PANFROST_DRM_H_

#include "drm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * 1. ARM Mali Architecture Constants & Product IDs
 * ========================================================================= */
#define MALI_PRODUCT_ID_T760            0x0750
#define MALI_PRODUCT_ID_T860            0x0860
#define MALI_PRODUCT_ID_G31             0x7093
#define MALI_PRODUCT_ID_G52             0x7212
#define MALI_PRODUCT_ID_G72             0x6221

/* Mali Job Types (Midgard / Bifrost Job Descriptors) */
#define MALI_JOB_TYPE_NOT_STARTED       0x00
#define MALI_JOB_TYPE_NULL              0x01
#define MALI_JOB_TYPE_WRITE_VALUE       0x02
#define MALI_JOB_TYPE_CACHE_FLUSH       0x03
#define MALI_JOB_TYPE_COMPUTE           0x04
#define MALI_JOB_TYPE_VERTEX            0x05
#define MALI_JOB_TYPE_TILER             0x06
#define MALI_JOB_TYPE_FUSED             0x07
#define MALI_JOB_TYPE_FRAGMENT          0x08

/* Mali Hardware MMIO Register Offsets */
#define MALI_REG_GPU_ID                 0x0000
#define MALI_REG_L2_FEATURES            0x0004
#define MALI_REG_SUSPEND_SIZE           0x0008
#define MALI_REG_TILER_FEATURES         0x000C
#define MALI_REG_MEM_FEATURES           0x0010
#define MALI_REG_MMU_FEATURES           0x0014
#define MALI_REG_AS_PRESENT             0x0018
#define MALI_REG_JS_PRESENT             0x001C
#define MALI_REG_SHADER_PRESENT_LO      0x0100
#define MALI_REG_SHADER_PRESENT_HI      0x0104
#define MALI_REG_TILER_PRESENT_LO       0x0110
#define MALI_REG_L2_PRESENT_LO          0x0120
#define MALI_REG_JOB_INT_RAWSTAT        0x1000
#define MALI_REG_JOB_INT_CLEAR          0x1004
#define MALI_REG_JOB_INT_MASK           0x1008
#define MALI_REG_JOB_INT_STATUS         0x100C
#define MALI_REG_JOB_SLOT_0_HEAD_LO     0x1080
#define MALI_REG_JOB_SLOT_0_HEAD_HI     0x1084
#define MALI_REG_JOB_SLOT_0_COMMAND     0x1090
#define MALI_REG_JOB_SLOT_0_STATUS      0x1094
#define MALI_REG_MMU_INT_RAWSTAT        0x2000
#define MALI_REG_MMU_INT_CLEAR          0x2004
#define MALI_REG_MMU_INT_MASK           0x2008

/* =========================================================================
 * 2. Panfrost Parameters
 * ========================================================================= */
#define DRM_PANFROST_PARAM_GPU_PROD_ID          0
#define DRM_PANFROST_PARAM_NUM_CORES            1
#define DRM_PANFROST_PARAM_SHADER_PRESENT       2
#define DRM_PANFROST_PARAM_TILER_FEATURES       3
#define DRM_PANFROST_PARAM_MEM_FEATURES         4
#define DRM_PANFROST_PARAM_MMU_FEATURES         5
#define DRM_PANFROST_PARAM_THREAD_TLS_ALLOC     6
#define DRM_PANFROST_PARAM_AFBC_FEATURES        7

struct drm_panfrost_get_param {
    uint64_t param;
    uint64_t value;
};

/* =========================================================================
 * 3. Panfrost Buffer Objects (BO)
 * ========================================================================= */
#define PANFROST_BO_NOEXEC                      (1 << 0)
#define PANFROST_BO_DONTNEED                    (1 << 1)

struct drm_panfrost_create_bo {
    uint32_t size;
    uint32_t flags;
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
};

struct drm_panfrost_mmap_bo {
    uint32_t handle;
    uint32_t flags;
    uint64_t offset;
};

struct drm_panfrost_get_bo_offset {
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
};

/* =========================================================================
 * 4. Panfrost Job Submission & Fences
 * ========================================================================= */
#define PANFROST_JD_REQ_FS                      (1 << 0)

struct drm_panfrost_submit {
    uint64_t jc;                        /* GPU address of head job descriptor */
    uint64_t in_syncs;
    uint32_t in_sync_count;
    uint32_t out_sync;
    uint64_t bo_handles;
    uint32_t bo_handle_count;
    uint32_t flags;
    uint32_t requirements;
};

struct drm_panfrost_wait_bo {
    uint32_t handle;
    uint32_t pad;
    int64_t  timeout_ns;
};

/* =========================================================================
 * 5. Panfrost IOCTL Commands
 * ========================================================================= */
#define DRM_PANFROST_SUBMIT             0x00
#define DRM_PANFROST_WAIT_BO            0x01
#define DRM_PANFROST_CREATE_BO          0x02
#define DRM_PANFROST_MMAP_BO            0x03
#define DRM_PANFROST_GET_PARAM          0x04
#define DRM_PANFROST_GET_BO_OFFSET      0x05

#define DRM_IOCTL_PANFROST_SUBMIT \
    _IOW(DRM_IOCTL_BASE, DRM_COMMAND_BASE + DRM_PANFROST_SUBMIT, struct drm_panfrost_submit)
#define DRM_IOCTL_PANFROST_WAIT_BO \
    _IOW(DRM_IOCTL_BASE, DRM_COMMAND_BASE + DRM_PANFROST_WAIT_BO, struct drm_panfrost_wait_bo)
#define DRM_IOCTL_PANFROST_CREATE_BO \
    _IOWR(DRM_IOCTL_BASE, DRM_COMMAND_BASE + DRM_PANFROST_CREATE_BO, struct drm_panfrost_create_bo)
#define DRM_IOCTL_PANFROST_MMAP_BO \
    _IOWR(DRM_IOCTL_BASE, DRM_COMMAND_BASE + DRM_PANFROST_MMAP_BO, struct drm_panfrost_mmap_bo)
#define DRM_IOCTL_PANFROST_GET_PARAM \
    _IOWR(DRM_IOCTL_BASE, DRM_COMMAND_BASE + DRM_PANFROST_GET_PARAM, struct drm_panfrost_get_param)
#define DRM_IOCTL_PANFROST_GET_BO_OFFSET \
    _IOWR(DRM_IOCTL_BASE, DRM_COMMAND_BASE + DRM_PANFROST_GET_BO_OFFSET, struct drm_panfrost_get_bo_offset)

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_PANFROST_DRM_H_ */
