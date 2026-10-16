/*
 * CherryDRM - Direct Rendering Manager Subsystem Acceptance Tests
 *
 * Verifies VirtIO-GPU, ARM Mali Panfrost, KMS, GEM & PRIME Buffer Sharing
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_drm.h"
#include "vfs_posix.h"
#include <string.h>

extern void cherry_drm_register_shell_commands(void);

static bool test_drm_version(void)
{
    struct drm_device *card = cherry_drm_get_primary();
    struct drm_device *render = cherry_drm_get_render();
    if (!card || !render) return false;

    char card_name[32] = {0};
    char card_desc[64] = {0};
    struct drm_version v_card;
    memset(&v_card, 0, sizeof(v_card));
    v_card.name = card_name;
    v_card.name_len = sizeof(card_name);
    v_card.desc = card_desc;
    v_card.desc_len = sizeof(card_desc);

    int ret1 = cherry_drm_ioctl(card, DRM_IOCTL_VERSION, &v_card);
    if (ret1 != 0 || strcmp(card_name, "virtio_gpu") != 0) return false;

    char render_name[32] = {0};
    struct drm_version v_render;
    memset(&v_render, 0, sizeof(v_render));
    v_render.name = render_name;
    v_render.name_len = sizeof(render_name);

    int ret2 = cherry_drm_ioctl(render, DRM_IOCTL_VERSION, &v_render);
    if (ret2 != 0 || strcmp(render_name, "panfrost") != 0) return false;

    return true;
}

static bool test_drm_capabilities(void)
{
    struct drm_device *card = cherry_drm_get_primary();
    if (!card) return false;

    struct drm_get_cap cap;
    cap.capability = DRM_CAP_DUMB_BUFFER;
    if (cherry_drm_ioctl(card, DRM_IOCTL_GET_CAP, &cap) != 0 || cap.value != 1) return false;

    cap.capability = DRM_CAP_PRIME;
    if (cherry_drm_ioctl(card, DRM_IOCTL_GET_CAP, &cap) != 0 || (cap.value & DRM_PRIME_CAP_IMPORT) == 0) return false;

    cap.capability = DRM_CAP_TIMESTAMP_MONOTONIC;
    if (cherry_drm_ioctl(card, DRM_IOCTL_GET_CAP, &cap) != 0 || cap.value != 1) return false;

    cap.capability = DRM_CAP_SYNCOBJ;
    if (cherry_drm_ioctl(card, DRM_IOCTL_GET_CAP, &cap) != 0 || cap.value != 1) return false;

    return true;
}

static bool test_gem_dumb_buffers(void)
{
    struct drm_device *card = cherry_drm_get_primary();
    if (!card) return false;

    /* 1. Create 640x480 @ 32bpp dumb buffer */
    struct drm_mode_create_dumb create_req;
    memset(&create_req, 0, sizeof(create_req));
    create_req.width = 640;
    create_req.height = 480;
    create_req.bpp = 32;

    int ret = cherry_drm_ioctl(card, DRM_IOCTL_MODE_CREATE_DUMB, &create_req);
    if (ret != 0 || create_req.handle == 0 || create_req.size < (640 * 480 * 4)) return false;

    /* 2. Map buffer to obtain virtual address */
    struct drm_mode_map_dumb map_req;
    memset(&map_req, 0, sizeof(map_req));
    map_req.handle = create_req.handle;

    ret = cherry_drm_ioctl(card, DRM_IOCTL_MODE_MAP_DUMB, &map_req);
    if (ret != 0 || map_req.offset == 0) return false;

    /* 3. Read/Write memory pattern */
    uint32_t *ptr = (uint32_t *)(uintptr_t)map_req.offset;
    ptr[0] = 0xAA55AA55;
    ptr[639] = 0x12345678;
    if (ptr[0] != 0xAA55AA55 || ptr[639] != 0x12345678) return false;

    /* 4. Destroy dumb buffer */
    struct drm_mode_destroy_dumb destroy_req;
    destroy_req.handle = create_req.handle;
    ret = cherry_drm_ioctl(card, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
    if (ret != 0) return false;

    return true;
}

static bool test_kms_pipeline(void)
{
    struct drm_device *card = cherry_drm_get_primary();
    if (!card) return false;

    /* 1. Get Resources */
    struct drm_mode_card_res res;
    memset(&res, 0, sizeof(res));
    if (cherry_drm_ioctl(card, DRM_IOCTL_MODE_GETRESOURCES, &res) != 0) return false;
    if (res.count_crtcs == 0 || res.count_connectors == 0 || res.count_encoders == 0) return false;

    /* 2. Get Connector */
    struct drm_mode_get_connector conn;
    memset(&conn, 0, sizeof(conn));
    conn.connector_id = card->connector.connector_id;
    if (cherry_drm_ioctl(card, DRM_IOCTL_MODE_GETCONNECTOR, &conn) != 0) return false;
    if (conn.connection != DRM_MODE_CONNECTED || conn.count_modes == 0) return false;

    /* 3. Create framebuffer and add to KMS */
    struct drm_mode_create_dumb c_dumb;
    memset(&c_dumb, 0, sizeof(c_dumb));
    c_dumb.width = 640;
    c_dumb.height = 480;
    c_dumb.bpp = 32;
    if (cherry_drm_ioctl(card, DRM_IOCTL_MODE_CREATE_DUMB, &c_dumb) != 0) return false;

    struct drm_mode_fb_cmd fb_cmd;
    memset(&fb_cmd, 0, sizeof(fb_cmd));
    fb_cmd.width = 640;
    fb_cmd.height = 480;
    fb_cmd.pitch = c_dumb.pitch;
    fb_cmd.bpp = 32;
    fb_cmd.depth = 24;
    fb_cmd.handle = c_dumb.handle;
    if (cherry_drm_ioctl(card, DRM_IOCTL_MODE_ADDFB, &fb_cmd) != 0 || fb_cmd.fb_id == 0) return false;

    /* 4. Set CRTC mode */
    struct drm_mode_crtc crtc_req;
    memset(&crtc_req, 0, sizeof(crtc_req));
    crtc_req.crtc_id = card->crtc.crtc_id;
    crtc_req.fb_id = fb_cmd.fb_id;
    crtc_req.mode_valid = 1;
    crtc_req.mode = card->connector.modes[0];
    if (cherry_drm_ioctl(card, DRM_IOCTL_MODE_SETCRTC, &crtc_req) != 0) return false;

    /* 5. Dirty FB & Page Flip */
    struct drm_mode_fb_dirty_cmd dirty;
    memset(&dirty, 0, sizeof(dirty));
    dirty.fb_id = fb_cmd.fb_id;
    if (cherry_drm_ioctl(card, DRM_IOCTL_MODE_DIRTYFB, &dirty) != 0) return false;

    struct drm_mode_crtc_page_flip flip;
    memset(&flip, 0, sizeof(flip));
    flip.crtc_id = card->crtc.crtc_id;
    flip.fb_id = fb_cmd.fb_id;
    if (cherry_drm_ioctl(card, DRM_IOCTL_MODE_PAGE_FLIP, &flip) != 0) return false;

    /* 6. Remove Framebuffer */
    uint32_t fb_id = fb_cmd.fb_id;
    if (cherry_drm_ioctl(card, DRM_IOCTL_MODE_RMFB, &fb_id) != 0) return false;

    /* 7. Destroy dumb buffer */
    struct drm_mode_destroy_dumb d_dumb;
    memset(&d_dumb, 0, sizeof(d_dumb));
    d_dumb.handle = c_dumb.handle;
    cherry_drm_ioctl(card, DRM_IOCTL_MODE_DESTROY_DUMB, &d_dumb);

    return true;
}

static bool test_virtio_gpu_ioctls(void)
{
    struct drm_device *card = cherry_drm_get_primary();
    if (!card) return false;

    /* Query 3D capabilities */
    struct drm_virtgpu_getparam param;
    param.param = VIRTGPU_PARAM_3D_FEATURES;
    param.value = 0;
    if (cherry_drm_ioctl(card, DRM_IOCTL_VIRTGPU_GETPARAM, &param) != 0) return false;

    /* Resource Create */
    struct drm_virtgpu_resource_create rc;
    memset(&rc, 0, sizeof(rc));
    rc.width = 512;
    rc.height = 512;
    rc.format = 1;
    if (cherry_drm_ioctl(card, DRM_IOCTL_VIRTGPU_RESOURCE_CREATE, &rc) != 0) return false;
    if (rc.bo_handle == 0 || rc.res_handle == 0) return false;

    /* Resource Map */
    struct drm_virtgpu_map m;
    memset(&m, 0, sizeof(m));
    m.handle = rc.bo_handle;
    if (cherry_drm_ioctl(card, DRM_IOCTL_VIRTGPU_MAP, &m) != 0 || m.offset == 0) return false;

    /* Clean up */
    struct drm_gem_close gc;
    gc.handle = rc.bo_handle;
    gc.pad = 0;
    cherry_drm_ioctl(card, DRM_IOCTL_GEM_CLOSE, &gc);

    return true;
}

static bool test_mali_panfrost_pipeline(void)
{
    struct drm_device *render = cherry_drm_get_render();
    if (!render) return false;

    /* 1. Query Mali GPU Specs */
    struct drm_panfrost_get_param param;
    param.param = DRM_PANFROST_PARAM_GPU_PROD_ID;
    if (cherry_drm_ioctl(render, DRM_IOCTL_PANFROST_GET_PARAM, &param) != 0) return false;
    if (param.value != MALI_PRODUCT_ID_G31) return false;

    param.param = DRM_PANFROST_PARAM_NUM_CORES;
    if (cherry_drm_ioctl(render, DRM_IOCTL_PANFROST_GET_PARAM, &param) != 0 || param.value != 4) return false;

    /* 2. Create Panfrost Buffer Object */
    struct drm_panfrost_create_bo create_bo;
    memset(&create_bo, 0, sizeof(create_bo));
    create_bo.size = 64 * 1024; /* 64KB vertex/texture buffer */
    create_bo.flags = 0;
    if (cherry_drm_ioctl(render, DRM_IOCTL_PANFROST_CREATE_BO, &create_bo) != 0 || create_bo.handle == 0) return false;

    /* 3. Map Panfrost BO */
    struct drm_panfrost_mmap_bo mmap_bo;
    memset(&mmap_bo, 0, sizeof(mmap_bo));
    mmap_bo.handle = create_bo.handle;
    if (cherry_drm_ioctl(render, DRM_IOCTL_PANFROST_MMAP_BO, &mmap_bo) != 0 || mmap_bo.offset == 0) return false;

    /* Write Mali Job Chain descriptor header */
    uint32_t *jc_header = (uint32_t *)(uintptr_t)mmap_bo.offset;
    jc_header[0] = MALI_JOB_TYPE_VERTEX;
    jc_header[1] = 0x00000001; /* next job */

    /* 4. Submit Job to Mali GPU */
    struct drm_panfrost_submit submit;
    memset(&submit, 0, sizeof(submit));
    submit.jc = create_bo.offset;
    submit.bo_handles = (uint64_t)(uintptr_t)&create_bo.handle;
    submit.bo_handle_count = 1;
    submit.requirements = 0;
    if (cherry_drm_ioctl(render, DRM_IOCTL_PANFROST_SUBMIT, &submit) != 0) return false;

    /* 5. Wait for BO completion */
    struct drm_panfrost_wait_bo wait_bo;
    wait_bo.handle = create_bo.handle;
    wait_bo.timeout_ns = 1000000;
    if (cherry_drm_ioctl(render, DRM_IOCTL_PANFROST_WAIT_BO, &wait_bo) != 0) return false;

    /* Clean up */
    struct drm_gem_close gc;
    gc.handle = create_bo.handle;
    gc.pad = 0;
    cherry_drm_ioctl(render, DRM_IOCTL_GEM_CLOSE, &gc);

    return true;
}

static bool test_prime_buffer_sharing(void)
{
    struct drm_device *card = cherry_drm_get_primary();
    struct drm_device *render = cherry_drm_get_render();
    if (!card || !render) return false;

    /* 1. Allocate BO on Mali Render node */
    struct drm_panfrost_create_bo c_bo;
    memset(&c_bo, 0, sizeof(c_bo));
    c_bo.size = 128 * 1024;
    if (cherry_drm_ioctl(render, DRM_IOCTL_PANFROST_CREATE_BO, &c_bo) != 0) return false;

    /* 2. Export Mali BO to PRIME DMA-BUF fd */
    struct drm_prime_handle prime_exp;
    memset(&prime_exp, 0, sizeof(prime_exp));
    prime_exp.handle = c_bo.handle;
    prime_exp.flags = DRM_CLOEXEC | DRM_RDWR;
    if (cherry_drm_ioctl(render, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime_exp) != 0 || prime_exp.fd <= 0) return false;

    /* 3. Import PRIME fd into VirtIO-GPU Primary node */
    struct drm_prime_handle prime_imp;
    memset(&prime_imp, 0, sizeof(prime_imp));
    prime_imp.fd = prime_exp.fd;
    if (cherry_drm_ioctl(card, DRM_IOCTL_PRIME_FD_TO_HANDLE, &prime_imp) != 0 || prime_imp.handle == 0) return false;

    /* Clean up handles */
    struct drm_gem_close gc1 = { .handle = c_bo.handle };
    struct drm_gem_close gc2 = { .handle = prime_imp.handle };
    cherry_drm_ioctl(render, DRM_IOCTL_GEM_CLOSE, &gc1);
    cherry_drm_ioctl(card, DRM_IOCTL_GEM_CLOSE, &gc2);

    return true;
}

static bool test_fbdev_emulation(void)
{
    /* Test opening and writing to /dev/fb0 via VFS */
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) return false;

    uint32_t test_pixels[16];
    for (int i = 0; i < 16; i++) test_pixels[i] = 0x00FF00FF;

    ssize_t w = write(fd, test_pixels, sizeof(test_pixels));
    if (w != (ssize_t)sizeof(test_pixels)) {
        close(fd);
        return false;
    }

    uint32_t read_pixels[16];
    lseek(fd, 0, SEEK_SET);
    ssize_t r = read(fd, read_pixels, sizeof(read_pixels));
    close(fd);

    if (r != (ssize_t)sizeof(read_pixels) || memcmp(test_pixels, read_pixels, sizeof(test_pixels)) != 0) {
        return false;
    }
    return true;
}


static bool test_bifrost_display_and_tiler(void)
{
    struct drm_device *render = cherry_drm_get_render();
    if (!render) return false;

    /* 1. Verify Bifrost UAPI Parameters */
    struct drm_panfrost_get_param param;
    param.param = DRM_PANFROST_PARAM_BIFROST_VERSION;
    if (cherry_drm_ioctl(render, DRM_IOCTL_PANFROST_GET_PARAM, &param) != 0 || param.value != 7) return false;

    param.param = DRM_PANFROST_PARAM_TILE_SIZE;
    if (cherry_drm_ioctl(render, DRM_IOCTL_PANFROST_GET_PARAM, &param) != 0 || param.value != 16) return false;

    param.param = DRM_PANFROST_PARAM_HAS_DISPLAY;
    if (cherry_drm_ioctl(render, DRM_IOCTL_PANFROST_GET_PARAM, &param) != 0 || param.value != 1) return false;

    /* 2. Test Panfrost Bifrost KMS Display Pipeline */
    struct drm_mode_card_res res;
    memset(&res, 0, sizeof(res));
    if (cherry_drm_ioctl(render, DRM_IOCTL_MODE_GETRESOURCES, &res) != 0) return false;
    if (res.count_crtcs == 0 || res.count_connectors == 0) return false;

    /* Create Dumb Scanout Buffer on Mali */
    struct drm_mode_create_dumb c_dumb;
    memset(&c_dumb, 0, sizeof(c_dumb));
    c_dumb.width = 640;
    c_dumb.height = 480;
    c_dumb.bpp = 32;
    if (cherry_drm_ioctl(render, DRM_IOCTL_MODE_CREATE_DUMB, &c_dumb) != 0) return false;

    struct drm_mode_fb_cmd fb_cmd;
    memset(&fb_cmd, 0, sizeof(fb_cmd));
    fb_cmd.width = 640;
    fb_cmd.height = 480;
    fb_cmd.pitch = c_dumb.pitch;
    fb_cmd.bpp = 32;
    fb_cmd.depth = 24;
    fb_cmd.handle = c_dumb.handle;
    if (cherry_drm_ioctl(render, DRM_IOCTL_MODE_ADDFB, &fb_cmd) != 0 || fb_cmd.fb_id == 0) return false;

    /* Set Panfrost CRTC */
    struct drm_mode_crtc crtc_req;
    memset(&crtc_req, 0, sizeof(crtc_req));
    crtc_req.crtc_id = render->crtc.crtc_id;
    crtc_req.fb_id = fb_cmd.fb_id;
    crtc_req.mode_valid = 1;
    crtc_req.mode = render->connector.modes[0];
    if (cherry_drm_ioctl(render, DRM_IOCTL_MODE_SETCRTC, &crtc_req) != 0) return false;

    /* Bifrost PageFlip & DirtyFB */
    struct drm_mode_fb_dirty_cmd dirty;
    memset(&dirty, 0, sizeof(dirty));
    dirty.fb_id = fb_cmd.fb_id;
    if (cherry_drm_ioctl(render, DRM_IOCTL_MODE_DIRTYFB, &dirty) != 0) return false;

    struct drm_mode_crtc_page_flip flip;
    memset(&flip, 0, sizeof(flip));
    flip.crtc_id = render->crtc.crtc_id;
    flip.fb_id = fb_cmd.fb_id;
    if (cherry_drm_ioctl(render, DRM_IOCTL_MODE_PAGE_FLIP, &flip) != 0) return false;

    /* Clean up */
    uint32_t fb_id = fb_cmd.fb_id;
    cherry_drm_ioctl(render, DRM_IOCTL_MODE_RMFB, &fb_id);
    struct drm_mode_destroy_dumb d_dumb = { .handle = c_dumb.handle };
    cherry_drm_ioctl(render, DRM_IOCTL_MODE_DESTROY_DUMB, &d_dumb);

    return true;
}

int cherry_drm_run_tests(void)
{
    pr_info("cherry_drm: starting acceptance test suite...\n");

    bool v_pass     = test_drm_version();
    bool c_pass     = test_drm_capabilities();
    bool gem_pass   = test_gem_dumb_buffers();
    bool kms_pass   = test_kms_pipeline();
    bool virt_pass  = test_virtio_gpu_ioctls();
    bool mali_pass  = test_mali_panfrost_pipeline();
    bool prime_pass = test_prime_buffer_sharing();
    bool fb_pass    = test_fbdev_emulation();

    bool bifrost_pass = test_bifrost_display_and_tiler();

    bool all_pass = v_pass && c_pass && gem_pass && kms_pass &&
                    virt_pass && mali_pass && prime_pass && fb_pass && bifrost_pass;

    printk("cherry_drm: [drm-verify] version=%s caps=%s gem=%s kms=%s virtio=%s mali=%s prime=%s fbdev=%s bifrost=%s all=%s\n",
           v_pass ? "pass" : "FAIL",
           c_pass ? "pass" : "FAIL",
           gem_pass ? "pass" : "FAIL",
           kms_pass ? "pass" : "FAIL",
           virt_pass ? "pass" : "FAIL",
           mali_pass ? "pass" : "FAIL",
           prime_pass ? "pass" : "FAIL",
           fb_pass ? "pass" : "FAIL",
           bifrost_pass ? "pass" : "FAIL",
           all_pass ? "pass" : "FAIL");

    /* Register shell commands */
    cherry_drm_register_shell_commands();

    return all_pass ? 0 : -1;
}
