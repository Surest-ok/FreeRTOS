/*
 * CherryDRM - Direct Rendering Manager for CherryOS / FreeRTOS
 *
 * Standard Linux DRM Core UAPI Definitions
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_DRM_H_
#define _CHERRY_DRM_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * 1. Linux IOCTL Encoding Macros
 * ========================================================================= */
#ifndef _IOC_NRBITS
#define _IOC_NRBITS   8
#define _IOC_TYPEBITS 8
#define _IOC_SIZEBITS 14
#define _IOC_DIRBITS  2

#define _IOC_NRMASK   ((1 << _IOC_NRBITS)-1)
#define _IOC_TYPEMASK ((1 << _IOC_TYPEBITS)-1)
#define _IOC_SIZEMASK ((1 << _IOC_SIZEBITS)-1)
#define _IOC_DIRMASK  ((1 << _IOC_DIRBITS)-1)

#define _IOC_NRSHIFT   0
#define _IOC_TYPESHIFT (_IOC_NRSHIFT+_IOC_NRBITS)
#define _IOC_SIZESHIFT (_IOC_TYPESHIFT+_IOC_TYPEBITS)
#define _IOC_DIRSHIFT  (_IOC_SIZESHIFT+_IOC_SIZEBITS)

#define _IOC_NONE      0U
#define _IOC_WRITE     1U
#define _IOC_READ      2U

#define _IOC(dir,type,nr,size) \
    (((dir)  << _IOC_DIRSHIFT) | \
     ((type) << _IOC_TYPESHIFT) | \
     ((nr)   << _IOC_NRSHIFT) | \
     ((size) << _IOC_SIZESHIFT))

#define _IO(type,nr)        _IOC(_IOC_NONE,(type),(nr),0)
#define _IOR(type,nr,size)  _IOC(_IOC_READ,(type),(nr),sizeof(size))
#define _IOW(type,nr,size)  _IOC(_IOC_WRITE,(type),(nr),sizeof(size))
#define _IOWR(type,nr,size) _IOC(_IOC_READ|_IOC_WRITE,(type),(nr),sizeof(size))

#define _IOC_NR(nr)         (((nr) >> _IOC_NRSHIFT) & _IOC_NRMASK)
#define _IOC_TYPE(nr)       (((nr) >> _IOC_TYPESHIFT) & _IOC_TYPEMASK)
#define _IOC_SIZE(nr)       (((nr) >> _IOC_SIZESHIFT) & _IOC_SIZEMASK)
#define _IOC_DIR(nr)        (((nr) >> _IOC_DIRSHIFT) & _IOC_DIRMASK)
#endif

#define DRM_IOCTL_BASE              'd'
#define DRM_COMMAND_BASE            0x40
#define DRM_COMMAND_END             0xA0

/* =========================================================================
 * 2. Core DRM Capabilities & Constants
 * ========================================================================= */
#define DRM_CAP_DUMB_BUFFER         0x1
#define DRM_CAP_VBLANK_HIGH_CRTC    0x2
#define DRM_CAP_DUMB_PREFERRED_DEPTH 0x3
#define DRM_CAP_DUMB_PREFER_SHADOW  0x4
#define DRM_CAP_PRIME               0x5
#define DRM_CAP_TIMESTAMP_MONOTONIC 0x6
#define DRM_CAP_ASYNC_PAGE_FLIP     0x7
#define DRM_CAP_CURSOR_WIDTH        0x8
#define DRM_CAP_CURSOR_HEIGHT       0x9
#define DRM_CAP_ADDFB2_MODIFIERS    0x10
#define DRM_CAP_PAGE_FLIP_TARGET    0x11
#define DRM_CAP_CRTC_IN_VBLANK_EVENT 0x12
#define DRM_CAP_SYNCOBJ             0x13

#define DRM_PRIME_CAP_IMPORT        0x1
#define DRM_PRIME_CAP_EXPORT        0x2

#define DRM_CLIENT_CAP_STEREO_3D        1
#define DRM_CLIENT_CAP_UNIVERSAL_PLANES 2
#define DRM_CLIENT_CAP_ATOMIC           3
#define DRM_CLIENT_CAP_ASPECT_RATIO     4
#define DRM_CLIENT_CAP_WRITEBACK_CONNECTORS 5

#define DRM_CLOEXEC                 02000000
#define DRM_RDWR                    02

/* =========================================================================
 * 3. Core DRM Data Structures
 * ========================================================================= */
struct drm_version {
    int      version_major;
    int      version_minor;
    int      version_patchlevel;
    size_t   name_len;
    char    *name;
    size_t   date_len;
    char    *date;
    size_t   desc_len;
    char    *desc;
};

struct drm_unique {
    size_t   unique_len;
    char    *unique;
};

struct drm_get_cap {
    uint64_t capability;
    uint64_t value;
};

struct drm_set_client_cap {
    uint64_t capability;
    uint64_t value;
};

struct drm_gem_close {
    uint32_t handle;
    uint32_t pad;
};

struct drm_prime_handle {
    uint32_t handle;
    uint32_t flags;
    int32_t  fd;
};

struct drm_syncobj_create {
    uint32_t handle;
    uint32_t flags;
};

struct drm_syncobj_destroy {
    uint32_t handle;
    uint32_t pad;
};

struct drm_syncobj_wait {
    uint64_t handles;
    int64_t  timeout_nsec;
    uint32_t count_handles;
    uint32_t flags;
    uint32_t first_signaled;
    uint32_t pad;
};

/* =========================================================================
 * 4. Core DRM IOCTLs
 * ========================================================================= */
#define DRM_IOCTL_VERSION               _IOWR(DRM_IOCTL_BASE, 0x00, struct drm_version)
#define DRM_IOCTL_GET_UNIQUE            _IOWR(DRM_IOCTL_BASE, 0x01, struct drm_unique)
#define DRM_IOCTL_GET_MAGIC             _IOR(DRM_IOCTL_BASE,  0x02, struct drm_auth)
#define DRM_IOCTL_GEM_CLOSE             _IOW(DRM_IOCTL_BASE,  0x09, struct drm_gem_close)
#define DRM_IOCTL_GET_CAP               _IOWR(DRM_IOCTL_BASE, 0x0c, struct drm_get_cap)
#define DRM_IOCTL_SET_CLIENT_CAP        _IOW(DRM_IOCTL_BASE,  0x0d, struct drm_set_client_cap)
#define DRM_IOCTL_PRIME_HANDLE_TO_FD    _IOWR(DRM_IOCTL_BASE, 0x2d, struct drm_prime_handle)
#define DRM_IOCTL_PRIME_FD_TO_HANDLE    _IOWR(DRM_IOCTL_BASE, 0x2e, struct drm_prime_handle)
#define DRM_IOCTL_SYNCOBJ_CREATE        _IOWR(DRM_IOCTL_BASE, 0xbf, struct drm_syncobj_create)
#define DRM_IOCTL_SYNCOBJ_DESTROY       _IOWR(DRM_IOCTL_BASE, 0xc0, struct drm_syncobj_destroy)
#define DRM_IOCTL_SYNCOBJ_WAIT          _IOWR(DRM_IOCTL_BASE, 0xc3, struct drm_syncobj_wait)

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_DRM_H_ */
