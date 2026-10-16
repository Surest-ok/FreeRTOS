/*
 * CherryDRM - Direct Rendering Manager Subsystem
 *
 * Linux Framebuffer (/dev/fb0) Emulation & Legacy Graphics Support
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_drm.h"
#include "vfs_dev.h"
#include "vfs_posix.h"
#include <string.h>

#define FBIOGET_VSCREENINFO     0x4600
#define FBIOPUT_VSCREENINFO     0x4601
#define FBIOGET_FSCREENINFO     0x4602
#define FBIO_PAN_DISPLAY        0x4606

struct fb_bitfield {
    uint32_t offset;
    uint32_t length;
    uint32_t msb_right;
};

struct fb_var_screeninfo {
    uint32_t xres;
    uint32_t yres;
    uint32_t xres_virtual;
    uint32_t yres_virtual;
    uint32_t xoffset;
    uint32_t yoffset;
    uint32_t bits_per_pixel;
    uint32_t grayscale;
    struct fb_bitfield red;
    struct fb_bitfield green;
    struct fb_bitfield blue;
    struct fb_bitfield transp;
    uint32_t nonstd;
    uint32_t activate;
    uint32_t height;
    uint32_t width;
    uint32_t accel_flags;
    uint32_t pixclock;
    uint32_t left_margin;
    uint32_t right_margin;
    uint32_t upper_margin;
    uint32_t lower_margin;
    uint32_t hsync_len;
    uint32_t vsync_len;
    uint32_t sync;
    uint32_t vmode;
    uint32_t rotate;
    uint32_t colorspace;
    uint32_t reserved[4];
};

struct fb_fix_screeninfo {
    char id[16];
    uint64_t smem_start;
    uint32_t smem_len;
    uint32_t type;
    uint32_t type_aux;
    uint32_t visual;
    uint16_t xpanstep;
    uint16_t ypanstep;
    uint16_t ywrapstep;
    uint32_t line_length;
    uint64_t mmio_start;
    uint32_t mmio_len;
    uint32_t accel;
    uint16_t capabilities;
    uint16_t reserved[2];
};

static struct drm_device *s_fbdev_drm_dev = NULL;
static struct vfs_cdev    s_fb0_cdev;
static uint8_t            s_fb0_fallback[64 * 1024];

static void *fb0_get_buffer(size_t *p_size)
{
    if (s_fbdev_drm_dev && s_fbdev_drm_dev->crtc.fb &&
        s_fbdev_drm_dev->crtc.fb->objs[0] && s_fbdev_drm_dev->crtc.fb->objs[0]->vaddr) {
        if (p_size) *p_size = s_fbdev_drm_dev->crtc.fb->objs[0]->size;
        return s_fbdev_drm_dev->crtc.fb->objs[0]->vaddr;
    }
    if (p_size) *p_size = sizeof(s_fb0_fallback);
    return s_fb0_fallback;
}

static int fb0_open(struct vfs_cdev *cdev, int flags)
{
    (void)cdev;
    (void)flags;
    return 0;
}

static int fb0_close(struct vfs_cdev *cdev)
{
    (void)cdev;
    return 0;
}

static ssize_t fb0_read(struct vfs_cdev *cdev, void *buf, size_t count)
{
    (void)cdev;
    size_t max_sz = 0;
    void *vaddr = fb0_get_buffer(&max_sz);
    if (!vaddr) return -ENODEV;

    size_t copy_sz = count < max_sz ? count : max_sz;
    memcpy(buf, vaddr, copy_sz);
    return copy_sz;
}

static ssize_t fb0_write(struct vfs_cdev *cdev, const void *buf, size_t count)
{
    (void)cdev;
    size_t max_sz = 0;
    void *vaddr = fb0_get_buffer(&max_sz);
    if (!vaddr) return -ENODEV;

    size_t copy_sz = count < max_sz ? count : max_sz;
    memcpy(vaddr, buf, copy_sz);
    return copy_sz;
}

static int fb0_ioctl(struct vfs_cdev *cdev, unsigned int cmd, unsigned long arg)
{
    (void)cdev;
    if (!s_fbdev_drm_dev) return -ENODEV;

    uint32_t width = s_fbdev_drm_dev->crtc.mode.hdisplay ? s_fbdev_drm_dev->crtc.mode.hdisplay : 1024;
    uint32_t height = s_fbdev_drm_dev->crtc.mode.vdisplay ? s_fbdev_drm_dev->crtc.mode.vdisplay : 768;
    uint32_t pitch = width * 4;

    switch (cmd) {
    case FBIOGET_VSCREENINFO: {
        struct fb_var_screeninfo *var = (struct fb_var_screeninfo *)arg;
        if (!var) return -EINVAL;
        memset(var, 0, sizeof(*var));
        var->xres = width;
        var->yres = height;
        var->xres_virtual = width;
        var->yres_virtual = height * 2; /* double buffered virtual height */
        var->bits_per_pixel = 32;
        var->red.offset = 16;
        var->red.length = 8;
        var->green.offset = 8;
        var->green.length = 8;
        var->blue.offset = 0;
        var->blue.length = 8;
        var->transp.offset = 24;
        var->transp.length = 8;
        return 0;
    }
    case FBIOPUT_VSCREENINFO:
        return 0;

    case FBIOGET_FSCREENINFO: {
        struct fb_fix_screeninfo *fix = (struct fb_fix_screeninfo *)arg;
        if (!fix) return -EINVAL;
        memset(fix, 0, sizeof(*fix));
        strncpy(fix->id, "cherry_drm_fb", sizeof(fix->id) - 1);
        size_t bsz = 0;
        void *smem = fb0_get_buffer(&bsz);
        fix->smem_start = (uint64_t)(uintptr_t)smem;
        fix->smem_len = (uint32_t)bsz;
        fix->type = 0; /* FB_TYPE_PACKED_PIXELS */
        fix->visual = 2; /* FB_VISUAL_TRUECOLOR */
        fix->line_length = pitch;
        return 0;
    }
    case FBIO_PAN_DISPLAY:
        return 0;
    default:
        return -ENOTTY;
    }
}

static const struct vfs_cdev_ops s_fb0_ops = {
    .open = fb0_open,
    .close = fb0_close,
    .read = fb0_read,
    .write = fb0_write,
    .ioctl = fb0_ioctl,
    .poll = NULL,
};

int cherry_fbdev_init(struct drm_device *dev)
{
    if (!dev) return -EINVAL;
    s_fbdev_drm_dev = dev;

    static bool s_fb0_registered = false;
    if (s_fb0_registered) return 0;

    memset(&s_fb0_cdev, 0, sizeof(s_fb0_cdev));
    s_fb0_cdev.ops = &s_fb0_ops;

    int ret = vfs_cdev_register(&s_fb0_cdev, "fb0");
    if (ret == 0) {
        s_fb0_registered = true;
        pr_info("cherry_drm: registered framebuffer console /dev/fb0 (%dx%d@32bpp)\n",
                dev->crtc.mode.hdisplay ? dev->crtc.mode.hdisplay : 1024,
                dev->crtc.mode.vdisplay ? dev->crtc.mode.vdisplay : 768);
    }
    return ret;
}
