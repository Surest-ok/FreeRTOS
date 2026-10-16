/*
 * CherryDRM - Direct Rendering Manager Subsystem
 *
 * CherryShell Interactive CLI Commands
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_drm.h"
#include "cherry_shell.h"
#include <string.h>

static int cmd_drm(int argc, char **argv)
{
    struct drm_device *card = cherry_drm_get_primary();
    struct drm_device *render = cherry_drm_get_render();

    if (argc < 2 || strcmp(argv[1], "info") == 0) {
        printk("=== CherryDRM Subsystem Status ===\n");
        if (card && card->driver) {
            printk("  [Primary Node] /dev/card0\n");
            printk("    Driver:      %s (%s v%d.%d.%d)\n",
                   card->driver->name, card->driver->desc,
                   card->driver->major, card->driver->minor, card->driver->patchlevel);
            printk("    CRTC Mode:   %s (%dx%d @ %dHz)\n",
                   card->crtc.mode.name, card->crtc.mode.hdisplay, card->crtc.mode.vdisplay, card->crtc.mode.vrefresh);
            printk("    Connector:   Virtual-1 (Status: Connected)\n");
            printk("    VBlank:      Seq=%lu, LastTime=%lu ns\n",
                   (unsigned long)card->vblank_sequence, (unsigned long)card->last_vblank_time_ns);
        }
        if (render && render->driver) {
            printk("  [Render Node]  /dev/renderD128\n");
            printk("    Driver:      %s (%s v%d.%d.%d)\n",
                   render->driver->name, render->driver->desc,
                   render->driver->major, render->driver->minor, render->driver->patchlevel);
        }
        return 0;
    }

    if (strcmp(argv[1], "modes") == 0) {
        if (!card) return -1;
        printk("=== Supported Display Modes ===\n");
        for (uint32_t i = 0; i < card->connector.count_modes; i++) {
            printk("  [%d] %-12s %dx%d @ %dHz (clock=%u kHz)\n",
                   i, card->connector.modes[i].name,
                   card->connector.modes[i].hdisplay, card->connector.modes[i].vdisplay,
                   card->connector.modes[i].vrefresh, card->connector.modes[i].clock);
        }
        return 0;
    }

    if (strcmp(argv[1], "test") == 0) {
        if (!card || !card->crtc.fb || !card->crtc.fb->objs[0]) {
            printk("drm: no active framebuffer found\n");
            return -1;
        }
        uint32_t *pixels = (uint32_t *)card->crtc.fb->objs[0]->vaddr;
        uint32_t w = card->crtc.mode.hdisplay;
        uint32_t h = card->crtc.mode.vdisplay;

        /* Render SMP test pattern / color bars: Red, Green, Blue, Yellow, Cyan, Magenta, White */
        uint32_t colors[7] = {
            0x00FF0000, 0x0000FF00, 0x000000FF,
            0x00FFFF00, 0x0000FFFF, 0x00FF00FF, 0x00FFFFFF
        };
        uint32_t bar_w = w / 7;

        for (uint32_t y = 0; y < h; y++) {
            for (uint32_t x = 0; x < w; x++) {
                uint32_t c_idx = (x / bar_w) % 7;
                pixels[y * w + x] = colors[c_idx];
            }
        }
        printk("drm: rendered 7-color test pattern to framebuffer (%dx%d)\n", w, h);
        return 0;
    }

    printk("usage: drm [info|modes|test]\n");
    return 0;
}

static int cmd_mali(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    struct drm_device *render = cherry_drm_get_render();
    if (!render) return -1;

    struct drm_panfrost_get_param param;
    param.param = DRM_PANFROST_PARAM_GPU_PROD_ID;
    cherry_drm_ioctl(render, DRM_IOCTL_PANFROST_GET_PARAM, &param);
    uint32_t prod_id = (uint32_t)param.value;

    param.param = DRM_PANFROST_PARAM_NUM_CORES;
    cherry_drm_ioctl(render, DRM_IOCTL_PANFROST_GET_PARAM, &param);
    uint32_t cores = (uint32_t)param.value;

    param.param = DRM_PANFROST_PARAM_SHADER_PRESENT;
    cherry_drm_ioctl(render, DRM_IOCTL_PANFROST_GET_PARAM, &param);
    uint32_t shader_present = (uint32_t)param.value;

    printk("=== ARM Mali Panfrost GPU Information ===\n");
    printk("  GPU Model:      Mali-G31 Bifrost v7 (Product ID: 0x%04x)\n", prod_id);
    printk("  Shader Cores:   %u active (mask: 0x%02x)\n", cores, shader_present);
    printk("  Architecture:   Bifrost Unified Geometry / Fragment Pipeline\n");
    printk("  Driver:         Panfrost DRM Driver (%s)\n", render->driver->name);
    return 0;
}

void cherry_drm_register_shell_commands(void)
{
    cherry_shell_register_symbol("drm", (uintptr_t)cmd_drm, CHERRY_SHELL_SYM_CMD, "Direct Rendering Manager commands: drm [info|modes|test]");
    cherry_shell_register_symbol("mali", (uintptr_t)cmd_mali, CHERRY_SHELL_SYM_CMD, "ARM Mali Panfrost GPU inspection: mali [info]");
}
