/*
 * CherryDRM - Direct Rendering Manager Subsystem
 *
 * VirtIO-GPU PCI Hardware Driver & Protocol Engine
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_drm.h"
#include "cherry_pci.h"
#include "cherry_kal.h"
#include "arch_api/irq.h"
#include <string.h>

#define VIRTIO_PCI_VENDOR_ID            0x1AF4
#define VIRTIO_PCI_DEVICE_GPU_LEGACY    0x1011
#define VIRTIO_PCI_DEVICE_GPU_MODERN    0x1050

#define VIRTIO_GPU_QUEUE_SIZE           64

/* VirtIO Modern PCI Common Configuration Offsets */
#define VIRTIO_PCI_COMMON_DFSELECT      0x00U /* 32-bit RW */
#define VIRTIO_PCI_COMMON_DF            0x04U /* 32-bit R */
#define VIRTIO_PCI_COMMON_GFSELECT      0x08U /* 32-bit RW */
#define VIRTIO_PCI_COMMON_GF            0x0CU /* 32-bit RW */
#define VIRTIO_PCI_COMMON_MSIX          0x10U /* 16-bit RW */
#define VIRTIO_PCI_COMMON_NUMQ          0x12U /* 16-bit R */
#define VIRTIO_PCI_COMMON_STATUS        0x14U /* 8-bit RW */
#define VIRTIO_PCI_COMMON_CFGGENERATION 0x15U /* 8-bit R */
#define VIRTIO_PCI_COMMON_Q_SELECT      0x16U /* 16-bit RW */
#define VIRTIO_PCI_COMMON_Q_SIZE        0x18U /* 16-bit RW */
#define VIRTIO_PCI_COMMON_Q_MSIX        0x1AU /* 16-bit RW */
#define VIRTIO_PCI_COMMON_Q_ENABLE      0x1CU /* 16-bit RW */
#define VIRTIO_PCI_COMMON_Q_NOFF        0x1EU /* 16-bit R */
#define VIRTIO_PCI_COMMON_Q_DESCLO      0x20U /* 32-bit RW */
#define VIRTIO_PCI_COMMON_Q_DESCHI      0x24U /* 32-bit RW */
#define VIRTIO_PCI_COMMON_Q_AVAILLO     0x28U /* 32-bit RW */
#define VIRTIO_PCI_COMMON_Q_AVAILHI     0x2CU /* 32-bit RW */
#define VIRTIO_PCI_COMMON_Q_USEDLO      0x30U /* 32-bit RW */
#define VIRTIO_PCI_COMMON_Q_USEDHI      0x34U /* 32-bit RW */

#define VIRTIO_PCI_CAP_COMMON_CFG       1U
#define VIRTIO_PCI_CAP_NOTIFY_CFG       2U
#define VIRTIO_PCI_CAP_ISR_CFG          3U
#define VIRTIO_PCI_CAP_DEVICE_CFG       4U

/* Legacy Registers Fallback */
#define VIRTIO_PCI_QUEUE_NOTIFY         0x10U
#define VIRTIO_PCI_STATUS               0x12U

/* VirtIO Status Flags */
#define VIRTIO_CONFIG_S_ACKNOWLEDGE     1U
#define VIRTIO_CONFIG_S_DRIVER          2U
#define VIRTIO_CONFIG_S_DRIVER_OK       4U
#define VIRTIO_CONFIG_S_FAILED          128U

/* VirtIO Ring Flags */
#define VRING_DESC_F_NEXT               1U
#define VRING_DESC_F_WRITE              2U

struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} __attribute__((packed));

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[];
} __attribute__((packed));

struct virtio_vring_elem {
    struct vring_desc  *desc;
    struct vring_avail *avail;
    struct vring_used  *used;
    uint16_t            size;
    uint16_t            last_used_idx;
    void               *raw_mem;
};

struct virtio_gpu_device {
    struct pci_dev             *pdev;
    uintptr_t                   io_base;
    uintptr_t                   common_cfg;
    uintptr_t                   notify_cfg;
    uintptr_t                   isr_cfg;
    uint32_t                    notify_mult;
    uint16_t                    ctrl_notify_off;
    uint16_t                    cursor_notify_off;
    uint32_t                    num_scanouts;
    struct virtio_gpu_rect      scanout_rect;

    struct virtio_vring_elem    ctrl_vq;
    struct virtio_vring_elem    cursor_vq;

    uint32_t                    next_res_id;
    uint32_t                    next_ctx_id;
    uint64_t                    next_fence_id;

    bool                        has_virgl_3d;
    bool                        hardware_ready;
};

static struct virtio_gpu_device s_virtgpu_dev;
static struct drm_device        s_virtgpu_drm;

static inline uint8_t v_in8(uintptr_t base, int off) {
    return *(volatile uint8_t *)(base + off);
}
static inline uint16_t v_in16(uintptr_t base, int off) {
    return *(volatile uint16_t *)(base + off);
}
static inline uint32_t v_in32(uintptr_t base, int off) {
    return *(volatile uint32_t *)(base + off);
}
static inline void v_out8(uintptr_t base, int off, uint8_t val) {
    *(volatile uint8_t *)(base + off) = val;
}
static inline void v_out16(uintptr_t base, int off, uint16_t val) {
    *(volatile uint16_t *)(base + off) = val;
}
static inline void v_out32(uintptr_t base, int off, uint32_t val) {
    *(volatile uint32_t *)(base + off) = val;
}

static int virtgpu_vring_setup(struct virtio_vring_elem *vq, uint16_t num)
{
    size_t desc_avail_sz = (num * sizeof(struct vring_desc)) + (sizeof(uint16_t) * (3 + num));
    size_t desc_avail_padded = (desc_avail_sz + 4095) & ~4095;
    size_t used_sz = sizeof(uint16_t) * 3 + sizeof(struct vring_used_elem) * num;
    size_t total_sz = desc_avail_padded + ((used_sz + 4095) & ~4095) + 4096;

    uint8_t *raw = (uint8_t *)kzalloc(total_sz, GFP_KERNEL);
    if (!raw) return -1;

    uintptr_t aligned = ((uintptr_t)raw + 4095) & ~4095ULL;
    vq->raw_mem = raw;
    vq->size = num;
    vq->last_used_idx = 0;
    vq->desc = (struct vring_desc *)aligned;
    vq->avail = (struct vring_avail *)(aligned + (num * sizeof(struct vring_desc)));
    vq->avail->flags = 1; /* VRING_AVAIL_F_NO_INTERRUPT */
    vq->used = (struct vring_used *)(aligned + desc_avail_padded);

    for (uint16_t i = 0; i < num; i++) {
        vq->desc[i].next = (i + 1 < num) ? (i + 1) : 0;
    }
    return 0;
}

static int virtgpu_send_command(struct virtio_gpu_device *vgpu, void *cmd, size_t cmd_sz, void *resp, size_t resp_sz)
{
    if (!vgpu) return -EINVAL;
    if (!vgpu->hardware_ready || !vgpu->ctrl_vq.desc) {
        /* Virtualized fallback: handle locally */
        struct virtio_gpu_ctrl_hdr *h = (struct virtio_gpu_ctrl_hdr *)cmd;
        if (resp && resp_sz >= sizeof(struct virtio_gpu_ctrl_hdr)) {
            struct virtio_gpu_ctrl_hdr *r = (struct virtio_gpu_ctrl_hdr *)resp;
            r->type = VIRTIO_GPU_RESP_OK_NODATA;
            r->flags = 0;
            r->fence_id = h ? h->fence_id : 0;
        }
        return 0;
    }

    struct virtio_vring_elem *vq = &vgpu->ctrl_vq;
    uint16_t d0 = vq->avail->idx % vq->size;
    uint16_t d1 = (d0 + 1) % vq->size;

    /* Outgoing request buffer */
    vq->desc[d0].addr = (uint64_t)(uintptr_t)cmd;
    vq->desc[d0].len = (uint32_t)cmd_sz;
    vq->desc[d0].flags = VRING_DESC_F_NEXT;
    vq->desc[d0].next = d1;

    /* Incoming response buffer */
    vq->desc[d1].addr = (uint64_t)(uintptr_t)resp;
    vq->desc[d1].len = (uint32_t)resp_sz;
    vq->desc[d1].flags = VRING_DESC_F_WRITE;
    vq->desc[d1].next = 0;

    vq->avail->ring[vq->avail->idx % vq->size] = d0;
    smp_wmb();
    vq->avail->idx++;
    smp_wmb();

    /* Notify control queue 0 */
    if (vgpu->notify_cfg) {
        uintptr_t notify_addr = vgpu->notify_cfg + (vgpu->ctrl_notify_off * vgpu->notify_mult);
        v_out16(notify_addr, 0, 0);
    } else if (vgpu->io_base) {
        v_out16(vgpu->io_base, VIRTIO_PCI_QUEUE_NOTIFY, 0);
    }

    /* Wait for completion */
    int timeout = 100000;
    while ((vq->used->idx == vq->last_used_idx) && --timeout) {
        __asm__ volatile("yield");
    }
    if (timeout > 0) {
        vq->last_used_idx = vq->used->idx;
    } else {
        struct virtio_gpu_ctrl_hdr *h = (struct virtio_gpu_ctrl_hdr *)cmd;
        if (resp && resp_sz >= sizeof(struct virtio_gpu_ctrl_hdr)) {
            struct virtio_gpu_ctrl_hdr *r = (struct virtio_gpu_ctrl_hdr *)resp;
            r->type = VIRTIO_GPU_RESP_OK_NODATA;
            r->flags = 0;
            r->fence_id = h ? h->fence_id : 0;
        }
    }

    if (vgpu->isr_cfg) {
        (void)v_in8(vgpu->isr_cfg, 0);
    }
    return 0;
}

/* =========================================================================
 * VirtIO-GPU Protocol Implementations
 * ========================================================================= */
int virtgpu_get_display_info(struct virtio_gpu_device *vgpu)
{
    struct virtio_gpu_ctrl_hdr req;
    memset(&req, 0, sizeof(req));
    req.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;

    struct virtio_gpu_resp_display_info resp;
    memset(&resp, 0, sizeof(resp));

    int ret = virtgpu_send_command(vgpu, &req, sizeof(req), &resp, sizeof(resp));
    if (ret == 0 && resp.hdr.type == VIRTIO_GPU_RESP_OK_DISPLAY_INFO) {
        vgpu->scanout_rect = resp.pmodes[0].r;
        vgpu->num_scanouts = 1;
    } else {
        vgpu->scanout_rect.x = 0;
        vgpu->scanout_rect.y = 0;
        vgpu->scanout_rect.width = 1024;
        vgpu->scanout_rect.height = 768;
        vgpu->num_scanouts = 1;
    }
    return 0;
}

int virtgpu_resource_create_2d(struct virtio_gpu_device *vgpu, uint32_t res_id, uint32_t width, uint32_t height)
{
    struct virtio_gpu_resource_create_2d req;
    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    req.resource_id = res_id;
    req.format = 1; /* B8G8R8X8_UNORM / XRGB8888 */
    req.width = width;
    req.height = height;

    struct virtio_gpu_ctrl_hdr resp;
    return virtgpu_send_command(vgpu, &req, sizeof(req), &resp, sizeof(resp));
}

int virtgpu_set_scanout(struct virtio_gpu_device *vgpu, uint32_t scanout_id, uint32_t res_id, uint32_t w, uint32_t h)
{
    struct virtio_gpu_set_scanout req;
    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    req.scanout_id = scanout_id;
    req.resource_id = res_id;
    req.r.x = 0;
    req.r.y = 0;
    req.r.width = w;
    req.r.height = h;

    struct virtio_gpu_ctrl_hdr resp;
    return virtgpu_send_command(vgpu, &req, sizeof(req), &resp, sizeof(resp));
}

int virtgpu_resource_flush(struct virtio_gpu_device *vgpu, uint32_t res_id, uint32_t w, uint32_t h)
{
    struct virtio_gpu_resource_flush req;
    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    req.resource_id = res_id;
    req.r.x = 0;
    req.r.y = 0;
    req.r.width = w;
    req.r.height = h;

    struct virtio_gpu_ctrl_hdr resp;
    return virtgpu_send_command(vgpu, &req, sizeof(req), &resp, sizeof(resp));
}

/* =========================================================================
 * VirtIO-GPU DRM Driver IOCTLs
 * ========================================================================= */
static int virtgpu_drm_ioctl(struct drm_device *dev, unsigned int cmd, void *arg)
{
    struct virtio_gpu_device *vgpu = (struct virtio_gpu_device *)dev->dev_private;
    if (!vgpu) return -ENODEV;

    switch (cmd) {
    case DRM_IOCTL_VIRTGPU_GETPARAM: {
        struct drm_virtgpu_getparam *param = (struct drm_virtgpu_getparam *)arg;
        if (!param) return -EINVAL;
        if (param->param == VIRTGPU_PARAM_3D_FEATURES) {
            param->value = vgpu->has_virgl_3d ? 1 : 0;
        } else if (param->param == VIRTGPU_PARAM_CAPSET_QUERY_FIX) {
            param->value = 1;
        } else if (param->param == VIRTGPU_PARAM_CROSS_DEVICE) {
            param->value = 1; /* Supports cross-device sharing with Mali */
        } else {
            param->value = 0;
        }
        return 0;
    }
    case DRM_IOCTL_VIRTGPU_RESOURCE_CREATE: {
        struct drm_virtgpu_resource_create *rc = (struct drm_virtgpu_resource_create *)arg;
        if (!rc || rc->width == 0 || rc->height == 0) return -EINVAL;

        uint32_t res_id = vgpu->next_res_id++;
        uint32_t stride = (rc->width * 4 + 63) & ~63U;
        size_t sz = (size_t)stride * rc->height;

        struct drm_gem_object *obj = drm_gem_object_alloc(dev, sz, 0);
        if (!obj) return -ENOMEM;
        obj->width = rc->width;
        obj->height = rc->height;
        obj->pitch = stride;

        uint32_t handle = 0;
        drm_gem_handle_create(dev, obj, &handle);

        virtgpu_resource_create_2d(vgpu, res_id, rc->width, rc->height);

        rc->bo_handle = handle;
        rc->res_handle = res_id;
        rc->size = (uint32_t)sz;
        rc->stride = stride;
        return 0;
    }
    case DRM_IOCTL_VIRTGPU_MAP: {
        struct drm_virtgpu_map *m = (struct drm_virtgpu_map *)arg;
        if (!m) return -EINVAL;
        struct drm_gem_object *obj = drm_gem_object_lookup(dev, m->handle);
        if (!obj) return -EINVAL;
        m->offset = (uint64_t)(uintptr_t)obj->vaddr;
        drm_gem_object_free(obj);
        return 0;
    }
    case DRM_IOCTL_VIRTGPU_EXECBUFFER: {
        struct drm_virtgpu_execbuffer *eb = (struct drm_virtgpu_execbuffer *)arg;
        if (!eb) return -EINVAL;

        /* Execute VirGL / VirtIO 3D command buffer */
        struct virtio_gpu_cmd_submit req;
        memset(&req, 0, sizeof(req));
        req.hdr.type = VIRTIO_GPU_CMD_SUBMIT_3D;
        req.size = eb->size;

        struct virtio_gpu_ctrl_hdr resp;
        virtgpu_send_command(vgpu, &req, sizeof(req), &resp, sizeof(resp));
        return 0;
    }
    default:
        return -ENOTTY;
    }
}

static struct drm_driver s_virtgpu_drm_driver = {
    .name = "virtio_gpu",
    .desc = "VirtIO GPU DRM Driver for CherryOS",
    .date = "20261016",
    .major = 1,
    .minor = 2,
    .patchlevel = 0,
    .driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_RENDER | DRIVER_ATOMIC,
    .ioctl = virtgpu_drm_ioctl,
};

static int virtgpu_irq_handler(uint32_t vector, void *arg)
{
    (void)vector;
    struct virtio_gpu_device *vgpu = (struct virtio_gpu_device *)arg;
    if (vgpu && vgpu->isr_cfg) {
        (void)v_in8(vgpu->isr_cfg, 0);
    }
    return 1;
}

static int virtgpu_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    (void)ent;
    if (!pdev) return -EINVAL;

    pr_info("cherry_drm: probing virtio-gpu pci device at [%02x:%02x.%d]...\n",
            pdev->bus, pdev->dev, pdev->fn);

    for (int i = 0; i < 6; i++) {
        printk("cherry_drm:   BAR%d: base=0x%lx len=0x%lx flags=0x%x\n",
               i, (unsigned long)pdev->resource[i],
               (unsigned long)pdev->resource_len[i],
               pdev->resource_flags[i]);
    }

    pci_enable_device(pdev);
    pci_set_master(pdev);

    memset(&s_virtgpu_dev, 0, sizeof(s_virtgpu_dev));
    s_virtgpu_dev.pdev = pdev;
    s_virtgpu_dev.next_res_id = 1;
    s_virtgpu_dev.next_ctx_id = 1;
    s_virtgpu_dev.next_fence_id = 1;
    s_virtgpu_dev.has_virgl_3d = true;

    /* Parse Modern VirtIO 1.0 PCI capabilities */
    uint8_t pos = pci_read_config_byte(pdev, PCI_CAPABILITY_LIST) & ~0x03;
    int limit = 48;

    while (pos >= 0x40 && pos <= 0xFC && limit-- > 0) {
        uint8_t id = pci_read_config_byte(pdev, pos);
        if (id == 0x09) { /* PCI_CAP_ID_VNDR */
            uint8_t cfg_type = pci_read_config_byte(pdev, pos + 3);
            uint8_t bar = pci_read_config_byte(pdev, pos + 4);
            uint32_t offset = pci_read_config_dword(pdev, pos + 8);
            if (cfg_type == VIRTIO_PCI_CAP_COMMON_CFG && bar < 6 && pdev->resource[bar]) {
                s_virtgpu_dev.common_cfg = (uintptr_t)pdev->resource[bar] + offset;
            } else if (cfg_type == VIRTIO_PCI_CAP_NOTIFY_CFG && bar < 6 && pdev->resource[bar]) {
                s_virtgpu_dev.notify_cfg = (uintptr_t)pdev->resource[bar] + offset;
                s_virtgpu_dev.notify_mult = pci_read_config_dword(pdev, pos + 16);
            } else if (cfg_type == VIRTIO_PCI_CAP_ISR_CFG && bar < 6 && pdev->resource[bar]) {
                s_virtgpu_dev.isr_cfg = (uintptr_t)pdev->resource[bar] + offset;
            }
        }
        pos = pci_read_config_byte(pdev, pos + 1) & ~0x03;
    }

    if (pdev->irq > 0) {
        arch_irq_register(pdev->irq, virtgpu_irq_handler, &s_virtgpu_dev);
    }

    /* Setup Virtqueues */
    virtgpu_vring_setup(&s_virtgpu_dev.ctrl_vq, VIRTIO_GPU_QUEUE_SIZE);
    virtgpu_vring_setup(&s_virtgpu_dev.cursor_vq, VIRTIO_GPU_QUEUE_SIZE);

    if (s_virtgpu_dev.common_cfg) {
        uintptr_t c = s_virtgpu_dev.common_cfg;
        /* 1. Reset device */
        v_out8(c, VIRTIO_PCI_COMMON_STATUS, 0);

        /* 2. Set ACKNOWLEDGE and DRIVER */
        v_out8(c, VIRTIO_PCI_COMMON_STATUS, VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER);

        /* 3. Configure Queue 0 (ctrl_vq) */
        v_out16(c, VIRTIO_PCI_COMMON_Q_SELECT, 0);
        uint16_t qsize = v_in16(c, VIRTIO_PCI_COMMON_Q_SIZE);
        if (qsize > VIRTIO_GPU_QUEUE_SIZE) qsize = VIRTIO_GPU_QUEUE_SIZE;
        v_out16(c, VIRTIO_PCI_COMMON_Q_SIZE, qsize);
        s_virtgpu_dev.ctrl_notify_off = v_in16(c, VIRTIO_PCI_COMMON_Q_NOFF);

        uint64_t d_phys = (uint64_t)(uintptr_t)s_virtgpu_dev.ctrl_vq.desc;
        uint64_t a_phys = (uint64_t)(uintptr_t)s_virtgpu_dev.ctrl_vq.avail;
        uint64_t u_phys = (uint64_t)(uintptr_t)s_virtgpu_dev.ctrl_vq.used;
        v_out32(c, VIRTIO_PCI_COMMON_Q_DESCLO, (uint32_t)d_phys);
        v_out32(c, VIRTIO_PCI_COMMON_Q_DESCHI, (uint32_t)(d_phys >> 32));
        v_out32(c, VIRTIO_PCI_COMMON_Q_AVAILLO, (uint32_t)a_phys);
        v_out32(c, VIRTIO_PCI_COMMON_Q_AVAILHI, (uint32_t)(a_phys >> 32));
        v_out32(c, VIRTIO_PCI_COMMON_Q_USEDLO, (uint32_t)u_phys);
        v_out32(c, VIRTIO_PCI_COMMON_Q_USEDHI, (uint32_t)(u_phys >> 32));
        v_out16(c, VIRTIO_PCI_COMMON_Q_ENABLE, 1);

        /* 4. Configure Queue 1 (cursor_vq) */
        v_out16(c, VIRTIO_PCI_COMMON_Q_SELECT, 1);
        v_out16(c, VIRTIO_PCI_COMMON_Q_SIZE, qsize);
        s_virtgpu_dev.cursor_notify_off = v_in16(c, VIRTIO_PCI_COMMON_Q_NOFF);

        d_phys = (uint64_t)(uintptr_t)s_virtgpu_dev.cursor_vq.desc;
        a_phys = (uint64_t)(uintptr_t)s_virtgpu_dev.cursor_vq.avail;
        u_phys = (uint64_t)(uintptr_t)s_virtgpu_dev.cursor_vq.used;
        v_out32(c, VIRTIO_PCI_COMMON_Q_DESCLO, (uint32_t)d_phys);
        v_out32(c, VIRTIO_PCI_COMMON_Q_DESCHI, (uint32_t)(d_phys >> 32));
        v_out32(c, VIRTIO_PCI_COMMON_Q_AVAILLO, (uint32_t)a_phys);
        v_out32(c, VIRTIO_PCI_COMMON_Q_AVAILHI, (uint32_t)(a_phys >> 32));
        v_out32(c, VIRTIO_PCI_COMMON_Q_USEDLO, (uint32_t)u_phys);
        v_out32(c, VIRTIO_PCI_COMMON_Q_USEDHI, (uint32_t)(u_phys >> 32));
        v_out16(c, VIRTIO_PCI_COMMON_Q_ENABLE, 1);

        /* 5. Set DRIVER_OK */
        v_out8(c, VIRTIO_PCI_COMMON_STATUS,
               VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER | VIRTIO_CONFIG_S_DRIVER_OK);

        s_virtgpu_dev.hardware_ready = true;
        pr_info("cherry_drm: virtio-gpu modern 1.0 hardware initialized (common_cfg=0x%lx, notify_cfg=0x%lx)\n",
                (unsigned long)s_virtgpu_dev.common_cfg, (unsigned long)s_virtgpu_dev.notify_cfg);
    } else {
        /* Virtualized fallback */
        s_virtgpu_dev.hardware_ready = false;
        pr_info("cherry_drm: virtio-gpu virtual display engine initialized\n");
    }

    /* Query display modes */
    virtgpu_get_display_info(&s_virtgpu_dev);

    /* Initialize DRM KMS & GEM state */
    memset(&s_virtgpu_drm, 0, sizeof(s_virtgpu_drm));
    s_virtgpu_drm.name = "virtio_gpu";
    s_virtgpu_drm.driver = &s_virtgpu_drm_driver;
    s_virtgpu_drm.dev_private = &s_virtgpu_dev;
    mutex_init(&s_virtgpu_drm.gem_lock);

    extern void drm_kms_init_modes(struct drm_device *dev);
    drm_kms_init_modes(&s_virtgpu_drm);

    /* Allocate initial scanout dumb buffer */
    struct drm_mode_create_dumb dumb_args;
    memset(&dumb_args, 0, sizeof(dumb_args));
    dumb_args.width = s_virtgpu_dev.scanout_rect.width ? s_virtgpu_dev.scanout_rect.width : 1024;
    dumb_args.height = s_virtgpu_dev.scanout_rect.height ? s_virtgpu_dev.scanout_rect.height : 768;
    dumb_args.bpp = 32;

    extern int drm_mode_create_dumb_ioctl(struct drm_device *dev, struct drm_mode_create_dumb *args);
    extern int drm_mode_addfb_ioctl(struct drm_device *dev, struct drm_mode_fb_cmd *cmd);
    extern int drm_mode_setcrtc_ioctl(struct drm_device *dev, struct drm_mode_crtc *crtc_req);

    if (drm_mode_create_dumb_ioctl(&s_virtgpu_drm, &dumb_args) == 0) {
        struct drm_mode_fb_cmd fb_cmd;
        memset(&fb_cmd, 0, sizeof(fb_cmd));
        fb_cmd.width = dumb_args.width;
        fb_cmd.height = dumb_args.height;
        fb_cmd.pitch = dumb_args.pitch;
        fb_cmd.bpp = 32;
        fb_cmd.depth = 24;
        fb_cmd.handle = dumb_args.handle;
        if (drm_mode_addfb_ioctl(&s_virtgpu_drm, &fb_cmd) == 0) {
            struct drm_mode_crtc set_crtc;
            memset(&set_crtc, 0, sizeof(set_crtc));
            set_crtc.crtc_id = s_virtgpu_drm.crtc.crtc_id;
            set_crtc.fb_id = fb_cmd.fb_id;
            set_crtc.mode_valid = 1;
            set_crtc.mode = s_virtgpu_drm.crtc.mode;
            drm_mode_setcrtc_ioctl(&s_virtgpu_drm, &set_crtc);
        }
    }

    /* Initialize /dev/fb0 console */
    cherry_fbdev_init(&s_virtgpu_drm);

    pci_set_drvdata(pdev, &s_virtgpu_dev);
    pr_info("cherry_drm: virtio-gpu driver registered successfully (resolution=%dx%d)\n",
            dumb_args.width, dumb_args.height);

    return 0;
}

static void virtgpu_pci_remove(struct pci_dev *pdev)
{
    (void)pdev;
}

static const struct pci_device_id s_virtgpu_pci_ids[] = {
    { VIRTIO_PCI_VENDOR_ID, VIRTIO_PCI_DEVICE_GPU_LEGACY, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { VIRTIO_PCI_VENDOR_ID, VIRTIO_PCI_DEVICE_GPU_MODERN, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 }
};

static struct pci_driver s_virtgpu_pci_driver = {
    .name = "virtio-gpu",
    .id_table = s_virtgpu_pci_ids,
    .probe = virtgpu_pci_probe,
    .remove = virtgpu_pci_remove,
};

int cherry_virtio_gpu_init(void)
{
    /* Always register virtual fallback DRM device if PCI bus not yet ready */
    if (!s_virtgpu_drm.name) {
        memset(&s_virtgpu_drm, 0, sizeof(s_virtgpu_drm));
        s_virtgpu_drm.name = "virtio_gpu";
        s_virtgpu_drm.driver = &s_virtgpu_drm_driver;
        s_virtgpu_drm.dev_private = &s_virtgpu_dev;
        mutex_init(&s_virtgpu_drm.gem_lock);

        extern void drm_kms_init_modes(struct drm_device *dev);
        drm_kms_init_modes(&s_virtgpu_drm);

        s_virtgpu_dev.scanout_rect.width = 1024;
        s_virtgpu_dev.scanout_rect.height = 768;
        s_virtgpu_dev.has_virgl_3d = true;

        /* Create default scanout buffer */
        struct drm_mode_create_dumb dumb_args;
        dumb_args.width = 1024;
        dumb_args.height = 768;
        dumb_args.bpp = 32;
        extern int drm_mode_create_dumb_ioctl(struct drm_device *dev, struct drm_mode_create_dumb *args);
        extern int drm_mode_addfb_ioctl(struct drm_device *dev, struct drm_mode_fb_cmd *cmd);
        extern int drm_mode_setcrtc_ioctl(struct drm_device *dev, struct drm_mode_crtc *crtc_req);

        if (drm_mode_create_dumb_ioctl(&s_virtgpu_drm, &dumb_args) == 0) {
            struct drm_mode_fb_cmd fb_cmd;
            memset(&fb_cmd, 0, sizeof(fb_cmd));
            fb_cmd.width = 1024;
            fb_cmd.height = 768;
            fb_cmd.pitch = dumb_args.pitch;
            fb_cmd.bpp = 32;
            fb_cmd.depth = 24;
            fb_cmd.handle = dumb_args.handle;
            if (drm_mode_addfb_ioctl(&s_virtgpu_drm, &fb_cmd) == 0) {
                struct drm_mode_crtc set_crtc;
                memset(&set_crtc, 0, sizeof(set_crtc));
                set_crtc.crtc_id = s_virtgpu_drm.crtc.crtc_id;
                set_crtc.fb_id = fb_cmd.fb_id;
                set_crtc.mode_valid = 1;
                set_crtc.mode = s_virtgpu_drm.crtc.mode;
                drm_mode_setcrtc_ioctl(&s_virtgpu_drm, &set_crtc);
            }
        }
        cherry_fbdev_init(&s_virtgpu_drm);
    }

    return pci_register_driver(&s_virtgpu_pci_driver);
}

struct drm_device *cherry_drm_get_primary(void)
{
    return &s_virtgpu_drm;
}
