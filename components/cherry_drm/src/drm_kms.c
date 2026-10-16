/*
 * CherryDRM - Direct Rendering Manager Subsystem
 *
 * Kernel Mode Setting (KMS) Engine: CRTC, Connector, Encoder & FB
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_drm.h"
#include <string.h>

extern struct drm_gem_object *drm_gem_object_lookup(struct drm_device *dev, uint32_t handle);
extern void drm_gem_object_free(struct drm_gem_object *obj);

void drm_kms_init_modes(struct drm_device *dev)
{
    if (!dev) return;

    /* Initialize Connector: Virtual-1 */
    struct drm_connector *conn = &dev->connector;
    conn->connector_id = 31;
    conn->connector_type = DRM_MODE_CONNECTOR_VIRTUAL;
    conn->encoder_id = 32;
    conn->connection = DRM_MODE_CONNECTED;
    conn->count_modes = 3;

    /* Mode 0: 1024x768 @ 60Hz */
    conn->modes[0].clock = 65000;
    conn->modes[0].hdisplay = 1024;
    conn->modes[0].hsync_start = 1048;
    conn->modes[0].hsync_end = 1184;
    conn->modes[0].htotal = 1344;
    conn->modes[0].vdisplay = 768;
    conn->modes[0].vsync_start = 771;
    conn->modes[0].vsync_end = 777;
    conn->modes[0].vtotal = 806;
    conn->modes[0].vrefresh = 60;
    conn->modes[0].type = DRM_MODE_TYPE_PREFERRED | DRM_MODE_TYPE_DRIVER;
    strncpy(conn->modes[0].name, "1024x768", DRM_DISPLAY_MODE_LEN - 1);

    /* Mode 1: 1280x720 @ 60Hz (720p) */
    conn->modes[1].clock = 74250;
    conn->modes[1].hdisplay = 1280;
    conn->modes[1].hsync_start = 1390;
    conn->modes[1].hsync_end = 1430;
    conn->modes[1].htotal = 1650;
    conn->modes[1].vdisplay = 720;
    conn->modes[1].vsync_start = 725;
    conn->modes[1].vsync_end = 730;
    conn->modes[1].vtotal = 750;
    conn->modes[1].vrefresh = 60;
    conn->modes[1].type = DRM_MODE_TYPE_DRIVER;
    strncpy(conn->modes[1].name, "1280x720", DRM_DISPLAY_MODE_LEN - 1);

    /* Mode 2: 1920x1080 @ 60Hz (1080p) */
    conn->modes[2].clock = 148500;
    conn->modes[2].hdisplay = 1920;
    conn->modes[2].hsync_start = 2008;
    conn->modes[2].hsync_end = 2052;
    conn->modes[2].htotal = 2200;
    conn->modes[2].vdisplay = 1080;
    conn->modes[2].vsync_start = 1084;
    conn->modes[2].vsync_end = 1089;
    conn->modes[2].vtotal = 1125;
    conn->modes[2].vrefresh = 60;
    conn->modes[2].type = DRM_MODE_TYPE_DRIVER;
    strncpy(conn->modes[2].name, "1920x1080", DRM_DISPLAY_MODE_LEN - 1);

    /* Initialize Encoder */
    struct drm_encoder *enc = &dev->encoder;
    enc->encoder_id = 32;
    enc->encoder_type = DRM_MODE_ENCODER_VIRTUAL;
    enc->crtc_id = 33;
    enc->possible_crtcs = 1;

    /* Initialize CRTC */
    struct drm_crtc *crtc = &dev->crtc;
    crtc->crtc_id = 33;
    crtc->enabled = true;
    crtc->mode = conn->modes[0]; /* Default 1024x768 */
    crtc->fb_id = 0;
    crtc->fb = NULL;

    dev->next_fb_id = 40;
}

int drm_mode_getresources_ioctl(struct drm_device *dev, struct drm_mode_card_res *res)
{
    if (!dev || !res) return -EINVAL;

    res->min_width = 320;
    res->max_width = 3840;
    res->min_height = 240;
    res->max_height = 2160;

    res->count_crtcs = 1;
    res->count_encoders = 1;
    res->count_connectors = 1;

    /* Count active framebuffers */
    uint32_t fb_count = 0;
    for (int i = 0; i < 8; i++) {
        if (dev->fbs[i].active) fb_count++;
    }
    res->count_fbs = fb_count;

    if (res->crtc_id_ptr) {
        uint32_t *ptr = (uint32_t *)(uintptr_t)res->crtc_id_ptr;
        ptr[0] = dev->crtc.crtc_id;
    }
    if (res->connector_id_ptr) {
        uint32_t *ptr = (uint32_t *)(uintptr_t)res->connector_id_ptr;
        ptr[0] = dev->connector.connector_id;
    }
    if (res->encoder_id_ptr) {
        uint32_t *ptr = (uint32_t *)(uintptr_t)res->encoder_id_ptr;
        ptr[0] = dev->encoder.encoder_id;
    }
    if (res->fb_id_ptr && fb_count > 0) {
        uint32_t *ptr = (uint32_t *)(uintptr_t)res->fb_id_ptr;
        int idx = 0;
        for (int i = 0; i < 8; i++) {
            if (dev->fbs[i].active) ptr[idx++] = dev->fbs[i].fb_id;
        }
    }

    return 0;
}

int drm_mode_getcrtc_ioctl(struct drm_device *dev, struct drm_mode_crtc *crtc_req)
{
    if (!dev || !crtc_req) return -EINVAL;
    if (crtc_req->crtc_id != dev->crtc.crtc_id) return -ENOENT;

    crtc_req->fb_id = dev->crtc.fb_id;
    crtc_req->x = dev->crtc.x;
    crtc_req->y = dev->crtc.y;
    crtc_req->gamma_size = 256;
    crtc_req->mode_valid = dev->crtc.enabled ? 1 : 0;
    crtc_req->mode = dev->crtc.mode;
    return 0;
}

int drm_mode_setcrtc_ioctl(struct drm_device *dev, struct drm_mode_crtc *crtc_req)
{
    if (!dev || !crtc_req) return -EINVAL;
    if (crtc_req->crtc_id != dev->crtc.crtc_id) return -ENOENT;

    dev->crtc.fb_id = crtc_req->fb_id;
    dev->crtc.x = crtc_req->x;
    dev->crtc.y = crtc_req->y;

    /* Update CRTC Framebuffer reference */
    dev->crtc.fb = NULL;
    for (int i = 0; i < 8; i++) {
        if (dev->fbs[i].active && dev->fbs[i].fb_id == crtc_req->fb_id) {
            dev->crtc.fb = &dev->fbs[i];
            break;
        }
    }

    if (crtc_req->mode_valid) {
        dev->crtc.mode = crtc_req->mode;
        dev->crtc.enabled = true;
    }

    return 0;
}

int drm_mode_getconnector_ioctl(struct drm_device *dev, struct drm_mode_get_connector *conn_req)
{
    if (!dev || !conn_req) return -EINVAL;
    if (conn_req->connector_id != dev->connector.connector_id) return -ENOENT;

    conn_req->connector_type = dev->connector.connector_type;
    conn_req->connector_type_id = 1;
    conn_req->connection = dev->connector.connection;
    conn_req->mm_width = 480;  /* ~22-inch display mm */
    conn_req->mm_height = 270;
    conn_req->subpixel = 1;    /* Horizontal RGB */
    conn_req->count_modes = dev->connector.count_modes;
    conn_req->count_encoders = 1;
    conn_req->encoder_id = dev->connector.encoder_id;

    if (conn_req->modes_ptr && conn_req->count_modes > 0) {
        struct drm_mode_modeinfo *ptr = (struct drm_mode_modeinfo *)(uintptr_t)conn_req->modes_ptr;
        for (uint32_t i = 0; i < dev->connector.count_modes; i++) {
            ptr[i] = dev->connector.modes[i];
        }
    }
    if (conn_req->encoders_ptr) {
        uint32_t *ptr = (uint32_t *)(uintptr_t)conn_req->encoders_ptr;
        ptr[0] = dev->encoder.encoder_id;
    }

    return 0;
}

int drm_mode_getencoder_ioctl(struct drm_device *dev, struct drm_mode_get_encoder *enc_req)
{
    if (!dev || !enc_req) return -EINVAL;
    if (enc_req->encoder_id != dev->encoder.encoder_id) return -ENOENT;

    enc_req->encoder_type = dev->encoder.encoder_type;
    enc_req->crtc_id = dev->encoder.crtc_id;
    enc_req->possible_crtcs = dev->encoder.possible_crtcs;
    enc_req->possible_clones = 0;
    return 0;
}

int drm_mode_addfb_ioctl(struct drm_device *dev, struct drm_mode_fb_cmd *cmd)
{
    if (!dev || !cmd || cmd->handle == 0) return -EINVAL;

    struct drm_gem_object *obj = drm_gem_object_lookup(dev, cmd->handle);
    if (!obj) return -EINVAL;

    /* Find empty framebuffer slot */
    struct drm_framebuffer *fb = NULL;
    for (int i = 0; i < 8; i++) {
        if (!dev->fbs[i].active) {
            fb = &dev->fbs[i];
            break;
        }
    }
    if (!fb) {
        drm_gem_object_free(obj);
        return -ENOSPC;
    }

    fb->fb_id = dev->next_fb_id++;
    fb->width = cmd->width;
    fb->height = cmd->height;
    fb->pixel_format = DRM_FORMAT_XRGB8888;
    fb->pitches[0] = cmd->pitch;
    fb->offsets[0] = 0;
    fb->objs[0] = obj;
    fb->dev = dev;
    fb->active = true;

    cmd->fb_id = fb->fb_id;
    return 0;
}

int drm_mode_addfb2_ioctl(struct drm_device *dev, struct drm_mode_fb_cmd2 *cmd2)
{
    if (!dev || !cmd2 || cmd2->handles[0] == 0) return -EINVAL;

    struct drm_gem_object *obj = drm_gem_object_lookup(dev, cmd2->handles[0]);
    if (!obj) return -EINVAL;

    struct drm_framebuffer *fb = NULL;
    for (int i = 0; i < 8; i++) {
        if (!dev->fbs[i].active) {
            fb = &dev->fbs[i];
            break;
        }
    }
    if (!fb) {
        drm_gem_object_free(obj);
        return -ENOSPC;
    }

    fb->fb_id = dev->next_fb_id++;
    fb->width = cmd2->width;
    fb->height = cmd2->height;
    fb->pixel_format = cmd2->pixel_format ? cmd2->pixel_format : DRM_FORMAT_XRGB8888;
    fb->pitches[0] = cmd2->pitches[0];
    fb->offsets[0] = cmd2->offsets[0];
    fb->objs[0] = obj;
    fb->dev = dev;
    fb->active = true;

    cmd2->fb_id = fb->fb_id;
    return 0;
}

int drm_mode_rmfb_ioctl(struct drm_device *dev, uint32_t fb_id)
{
    if (!dev || fb_id == 0) return -EINVAL;

    for (int i = 0; i < 8; i++) {
        if (dev->fbs[i].active && dev->fbs[i].fb_id == fb_id) {
            if (dev->fbs[i].objs[0]) {
                drm_gem_object_free(dev->fbs[i].objs[0]);
                dev->fbs[i].objs[0] = NULL;
            }
            dev->fbs[i].active = false;
            if (dev->crtc.fb_id == fb_id) {
                dev->crtc.fb_id = 0;
                dev->crtc.fb = NULL;
            }
            return 0;
        }
    }
    return -ENOENT;
}

int drm_mode_dirtyfb_ioctl(struct drm_device *dev, struct drm_mode_fb_dirty_cmd *cmd)
{
    if (!dev || !cmd) return -EINVAL;

    /* Flush modified regions to VirtIO display scanout if supported */
    struct drm_framebuffer *fb = NULL;
    for (int i = 0; i < 8; i++) {
        if (dev->fbs[i].active && dev->fbs[i].fb_id == cmd->fb_id) {
            fb = &dev->fbs[i];
            break;
        }
    }
    if (!fb) return -ENOENT;

    /* Trigger page flip or flush hardware display */
    dev->vblank_sequence++;
    dev->last_vblank_time_ns = ktime_get_ns();
    return 0;
}

int drm_mode_page_flip_ioctl(struct drm_device *dev, struct drm_mode_crtc_page_flip *page_flip)
{
    if (!dev || !page_flip) return -EINVAL;
    if (page_flip->crtc_id != dev->crtc.crtc_id) return -ENOENT;

    /* Atomic flip: switch CRTC fb to target fb */
    dev->crtc.fb_id = page_flip->fb_id;
    for (int i = 0; i < 8; i++) {
        if (dev->fbs[i].active && dev->fbs[i].fb_id == page_flip->fb_id) {
            dev->crtc.fb = &dev->fbs[i];
            break;
        }
    }

    dev->vblank_sequence++;
    dev->last_vblank_time_ns = ktime_get_ns();
    return 0;
}
