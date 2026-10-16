/*
 * CherryDRM - Direct Rendering Manager for CherryOS / FreeRTOS
 *
 * Standard Linux Kernel Mode Setting (KMS) UAPI Definitions
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_DRM_MODE_H_
#define _CHERRY_DRM_MODE_H_

#include "drm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DRM_DISPLAY_MODE_LEN    32
#define DRM_PROP_NAME_LEN       32

#define DRM_MODE_TYPE_BUILTIN   (1<<0)
#define DRM_MODE_TYPE_CLOCK_C   ((1<<1) | DRM_MODE_TYPE_BUILTIN)
#define DRM_MODE_TYPE_CRTC_C    ((1<<2) | DRM_MODE_TYPE_BUILTIN)
#define DRM_MODE_TYPE_PREFERRED (1<<3)
#define DRM_MODE_TYPE_DEFAULT   (1<<4)
#define DRM_MODE_TYPE_USERDEF   (1<<5)
#define DRM_MODE_TYPE_DRIVER    (1<<6)

/* Video Timing Flags */
#define DRM_MODE_FLAG_PHSYNC    (1<<0)
#define DRM_MODE_FLAG_NHSYNC    (1<<1)
#define DRM_MODE_FLAG_PVSYNC    (1<<2)
#define DRM_MODE_FLAG_NVSYNC    (1<<3)
#define DRM_MODE_FLAG_INTERLACE (1<<4)

/* Connector Types */
#define DRM_MODE_CONNECTOR_Unknown      0
#define DRM_MODE_CONNECTOR_VGA          1
#define DRM_MODE_CONNECTOR_DVII         2
#define DRM_MODE_CONNECTOR_DVID         3
#define DRM_MODE_CONNECTOR_DVIA         4
#define DRM_MODE_CONNECTOR_Composite    5
#define DRM_MODE_CONNECTOR_SVIDEO       6
#define DRM_MODE_CONNECTOR_LVDS         7
#define DRM_MODE_CONNECTOR_Component    8
#define DRM_MODE_CONNECTOR_9PinDIN      9
#define DRM_MODE_CONNECTOR_DisplayPort  10
#define DRM_MODE_CONNECTOR_HDMIA        11
#define DRM_MODE_CONNECTOR_HDMIB        12
#define DRM_MODE_CONNECTOR_TV           13
#define DRM_MODE_CONNECTOR_eDP          14
#define DRM_MODE_CONNECTOR_VIRTUAL      15
#define DRM_MODE_CONNECTOR_DSI          16
#define DRM_MODE_CONNECTOR_DPI          17
#define DRM_MODE_CONNECTOR_WRITEBACK    18
#define DRM_MODE_CONNECTOR_SPI          19

#define DRM_MODE_CONNECTED             1
#define DRM_MODE_DISCONNECTED          2
#define DRM_MODE_UNKNOWNCONNECTION     3

/* Encoder Types */
#define DRM_MODE_ENCODER_NONE          0
#define DRM_MODE_ENCODER_DAC           1
#define DRM_MODE_ENCODER_TMDS          2
#define DRM_MODE_ENCODER_LVDS          3
#define DRM_MODE_ENCODER_TVDAC         4
#define DRM_MODE_ENCODER_VIRTUAL       5
#define DRM_MODE_ENCODER_DSI           6
#define DRM_MODE_ENCODER_DPMST         7
#define DRM_MODE_ENCODER_DPI           8

/* FourCC Pixel Format Definitions */
#define fourcc_code(a, b, c, d) \
    ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

#define DRM_FORMAT_RGB565       fourcc_code('R', 'G', '1', '6') /* [15:0] R:G:B 5:6:5 little endian */
#define DRM_FORMAT_BGR565       fourcc_code('B', 'G', '1', '6') /* [15:0] B:G:R 5:6:5 little endian */
#define DRM_FORMAT_XRGB8888     fourcc_code('X', 'R', '2', '4') /* [31:0] x:R:G:B 8:8:8:8 little endian */
#define DRM_FORMAT_XBGR8888     fourcc_code('X', 'B', '2', '4') /* [31:0] x:B:G:R 8:8:8:8 little endian */
#define DRM_FORMAT_RGBX8888     fourcc_code('R', 'X', '2', '4') /* [31:0] R:G:B:x 8:8:8:8 little endian */
#define DRM_FORMAT_BGRX8888     fourcc_code('B', 'X', '2', '4') /* [31:0] B:G:R:x 8:8:8:8 little endian */
#define DRM_FORMAT_ARGB8888     fourcc_code('A', 'R', '2', '4') /* [31:0] A:R:G:B 8:8:8:8 little endian */
#define DRM_FORMAT_ABGR8888     fourcc_code('A', 'B', '2', '4') /* [31:0] A:B:G:R 8:8:8:8 little endian */
#define DRM_FORMAT_RGBA8888     fourcc_code('R', 'A', '2', '4') /* [31:0] R:G:B:A 8:8:8:8 little endian */
#define DRM_FORMAT_BGRA8888     fourcc_code('B', 'A', '2', '4') /* [31:0] B:G:R:A 8:8:8:8 little endian */

/* Mode Information Structure */
struct drm_mode_modeinfo {
    uint32_t clock;
    uint16_t hdisplay;
    uint16_t hsync_start;
    uint16_t hsync_end;
    uint16_t htotal;
    uint16_t hskew;
    uint16_t vdisplay;
    uint16_t vsync_start;
    uint16_t vsync_end;
    uint16_t vtotal;
    uint16_t vscan;

    uint32_t vrefresh;

    uint32_t flags;
    uint32_t type;
    char name[DRM_DISPLAY_MODE_LEN];
};

/* KMS Card Resources */
struct drm_mode_card_res {
    uint64_t fb_id_ptr;
    uint64_t crtc_id_ptr;
    uint64_t connector_id_ptr;
    uint64_t encoder_id_ptr;
    uint32_t count_fbs;
    uint32_t count_crtcs;
    uint32_t count_connectors;
    uint32_t count_encoders;
    uint32_t min_width;
    uint32_t max_width;
    uint32_t min_height;
    uint32_t max_height;
};

/* CRTC Configuration */
struct drm_mode_crtc {
    uint64_t set_connectors_ptr;
    uint32_t count_connectors;

    uint32_t crtc_id;
    uint32_t fb_id;

    uint32_t x;
    uint32_t y;

    uint32_t gamma_size;
    uint32_t mode_valid;
    struct drm_mode_modeinfo mode;
};

/* Connector Information */
struct drm_mode_get_connector {
    uint64_t encoders_ptr;
    uint64_t modes_ptr;
    uint64_t props_ptr;
    uint64_t prop_values_ptr;

    uint32_t count_modes;
    uint32_t count_props;
    uint32_t count_encoders;

    uint32_t encoder_id;
    uint32_t connector_id;
    uint32_t connector_type;
    uint32_t connector_type_id;

    uint32_t connection;
    uint32_t mm_width;
    uint32_t mm_height;
    uint32_t subpixel;

    uint32_t pad;
};

/* Encoder Information */
struct drm_mode_get_encoder {
    uint32_t encoder_id;
    uint32_t encoder_type;

    uint32_t crtc_id;

    uint32_t possible_crtcs;
    uint32_t possible_clones;
};

/* Framebuffer Creation (Legacy & Modern) */
struct drm_mode_fb_cmd {
    uint32_t fb_id;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t depth;
    uint32_t handle;
};

struct drm_mode_fb_cmd2 {
    uint32_t fb_id;
    uint32_t width;
    uint32_t height;
    uint32_t pixel_format;
    uint32_t flags;

    uint32_t handles[4];
    uint32_t pitches[4];
    uint32_t offsets[4];
    uint64_t modifier[4];
};

/* Dumb Buffer Memory Allocation & Mapping */
struct drm_mode_create_dumb {
    uint32_t height;
    uint32_t width;
    uint32_t bpp;
    uint32_t flags;
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
};

struct drm_mode_map_dumb {
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
};

struct drm_mode_destroy_dumb {
    uint32_t handle;
};

/* Page Flip & Dirty Framebuffer */
#define DRM_MODE_PAGE_FLIP_EVENT 0x01
#define DRM_MODE_PAGE_FLIP_ASYNC 0x02

struct drm_mode_crtc_page_flip {
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t flags;
    uint32_t reserved;
    uint64_t user_data;
};

struct drm_mode_fb_dirty_cmd {
    uint32_t fb_id;
    uint32_t flags;
    uint32_t color;
    uint32_t num_clips;
    uint64_t clips_ptr;
};

struct drm_clip_rect {
    unsigned short x1;
    unsigned short y1;
    unsigned short x2;
    unsigned short y2;
};

/* Plane Resources */
struct drm_mode_get_plane_res {
    uint64_t plane_id_ptr;
    uint32_t count_planes;
};

struct drm_mode_get_plane {
    uint32_t plane_id;

    uint32_t crtc_id;
    uint32_t fb_id;

    uint32_t possible_crtcs;
    uint32_t gamma_size;

    uint32_t count_format_types;
    uint64_t format_type_ptr;
};

struct drm_mode_set_plane {
    uint32_t plane_id;
    uint32_t crtc_id;
    uint32_t fb_id;

    uint32_t flags;

    /* Signed destination coordinates */
    int32_t crtc_x;
    int32_t crtc_y;
    uint32_t crtc_w;
    uint32_t crtc_h;

    /* Source coordinates in 16.16 fixed point */
    uint32_t src_x;
    uint32_t src_y;
    uint32_t src_h;
    uint32_t src_w;
};

/* =========================================================================
 * KMS IOCTL Commands
 * ========================================================================= */
#define DRM_IOCTL_MODE_GETRESOURCES     _IOWR(DRM_IOCTL_BASE, 0xA0, struct drm_mode_card_res)
#define DRM_IOCTL_MODE_GETCRTC          _IOWR(DRM_IOCTL_BASE, 0xA1, struct drm_mode_crtc)
#define DRM_IOCTL_MODE_SETCRTC          _IOWR(DRM_IOCTL_BASE, 0xA2, struct drm_mode_crtc)
#define DRM_IOCTL_MODE_GETENCODER       _IOWR(DRM_IOCTL_BASE, 0xA6, struct drm_mode_get_encoder)
#define DRM_IOCTL_MODE_GETCONNECTOR     _IOWR(DRM_IOCTL_BASE, 0xA7, struct drm_mode_get_connector)
#define DRM_IOCTL_MODE_ADDFB            _IOWR(DRM_IOCTL_BASE, 0xAE, struct drm_mode_fb_cmd)
#define DRM_IOCTL_MODE_ADDFB2           _IOWR(DRM_IOCTL_BASE, 0xB8, struct drm_mode_fb_cmd2)
#define DRM_IOCTL_MODE_RMFB             _IOWR(DRM_IOCTL_BASE, 0xAF, unsigned int)
#define DRM_IOCTL_MODE_PAGE_FLIP        _IOWR(DRM_IOCTL_BASE, 0xB0, struct drm_mode_crtc_page_flip)
#define DRM_IOCTL_MODE_DIRTYFB          _IOWR(DRM_IOCTL_BASE, 0xB1, struct drm_mode_fb_dirty_cmd)
#define DRM_IOCTL_MODE_CREATE_DUMB      _IOWR(DRM_IOCTL_BASE, 0xB2, struct drm_mode_create_dumb)
#define DRM_IOCTL_MODE_MAP_DUMB         _IOWR(DRM_IOCTL_BASE, 0xB3, struct drm_mode_map_dumb)
#define DRM_IOCTL_MODE_DESTROY_DUMB     _IOWR(DRM_IOCTL_BASE, 0xB4, struct drm_mode_destroy_dumb)
#define DRM_IOCTL_MODE_GETPLANERESOURCES _IOWR(DRM_IOCTL_BASE, 0xB5, struct drm_mode_get_plane_res)
#define DRM_IOCTL_MODE_GETPLANE         _IOWR(DRM_IOCTL_BASE, 0xB6, struct drm_mode_get_plane)
#define DRM_IOCTL_MODE_SETPLANE         _IOWR(DRM_IOCTL_BASE, 0xB7, struct drm_mode_set_plane)

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_DRM_MODE_H_ */
