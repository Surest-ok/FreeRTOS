/*
 * CherryMesa - 3D Graphics Library
 *
 * CherryShell Interactive CLI Commands & Subsystem Initialization
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_mesa.h"
#include "cherry_shell.h"
#include "cherry_kal.h"
#include <string.h>

static int parse_int(const char *str, int def_val)
{
    if (!str || !*str) return def_val;
    int res = 0;
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res > 0 ? res : def_val;
}

static int cmd_mesa(int argc, char **argv)
{
    if (argc < 2 || strcmp(argv[1], "info") == 0) {
        EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(dpy, NULL, NULL);

        printk("=== CherryMesa 3D Graphics Subsystem ===\n");
        printk("  EGL Version:     %s\n", eglQueryString(dpy, EGL_VERSION));
        printk("  EGL Vendor:      %s\n", eglQueryString(dpy, EGL_VENDOR));
        printk("  EGL APIs:        %s\n", eglQueryString(dpy, EGL_CLIENT_APIS));
        printk("  GL Renderer:     %s\n", (const char *)glGetString(GL_RENDERER));
        printk("  GL Version:      %s\n", (const char *)glGetString(GL_VERSION));
        printk("  GLSL Version:    %s\n", (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION));
        printk("  Gallium Driver:  Panfrost (Mali Bifrost) / VirGL / Softpipe\n");
        printk("  GBM Support:     Yes (Zero-Copy PRIME DMA-BUF GEM Sharing)\n");
        return 0;
    }

    if (strcmp(argv[1], "triangle") == 0) {
        int frames = 10;
        if (argc >= 3) {
            frames = parse_int(argv[2], 10);
        }
        printk("Running rotating 3D colored triangle demo (%d frames)...\n", frames);
        return cherry_mesa_run_triangle(frames);
    }

    if (strcmp(argv[1], "gears") == 0) {
        int frames = 10;
        if (argc >= 3) {
            frames = parse_int(argv[2], 10);
        }
        printk("Running classical 3D es2gears benchmark (%d frames)...\n", frames);
        return cherry_mesa_run_gears(frames);
    }

    if (strcmp(argv[1], "test") == 0) {
        return cherry_mesa_run_tests();
    }

    printk("usage: mesa [info|triangle [frames]|gears [frames]|test]\n");
    return 0;
}

static int cmd_es2gears(int argc, char **argv)
{
    int frames = 60;
    if (argc >= 2) {
        frames = parse_int(argv[1], 60);
    }
    printk("Running classical 3D es2gears benchmark (%d frames)...\n", frames);
    return cherry_mesa_run_gears(frames);
}


static int cmd_bifrost(int argc, char **argv)
{
    if (argc < 2 || strcmp(argv[1], "info") == 0) {
        printk("=== ARM Mali Panfrost Bifrost Graphics Engine ===\n");
        printk("  GPU Target:      ARM Mali-G31 Bifrost v7\n");
        printk("  Tiler Hierarchy: 16x16 Pixel Tile Buffer\n");
        printk("  Shading Engine:  Instruction Clauses (ALU, Texture, Load/Store)\n");
        printk("  Compression:     AFBC v1.2 (ARM Framebuffer Compression)\n");
        printk("  Display Scanout: Mali-DP / KMS Active (/dev/card1)\n");
        return 0;
    }

    if (strcmp(argv[1], "triangle") == 0) {
        int frames = 10;
        if (argc >= 3) frames = parse_int(argv[2], 10);
        printk("Running Panfrost Bifrost 3D triangle demo (%d frames)...\n", frames);
        return cherry_mesa_run_bifrost_triangle(frames);
    }

    if (strcmp(argv[1], "gears") == 0) {
        int frames = 10;
        if (argc >= 3) frames = parse_int(argv[2], 10);
        printk("Running Panfrost Bifrost es2gears benchmark (%d frames)...\n", frames);
        return cherry_mesa_run_bifrost_gears(frames);
    }

    if (strcmp(argv[1], "test") == 0) {
        int r1 = cherry_mesa_run_bifrost_triangle(2);
        int r2 = cherry_mesa_run_bifrost_gears(2);
        printk("bifrost: test %s\n", (r1 == 0 && r2 == 0) ? "PASSED" : "FAILED");
        return (r1 == 0 && r2 == 0) ? 0 : -1;
    }

    printk("usage: bifrost [info|triangle [frames]|gears [frames]|test]\n");
    return 0;
}

int cherry_mesa_init(void)
{
    static bool s_inited = false;
    if (s_inited) return 0;

    pr_info("cherry_mesa: Gallium3D Mesa subsystem initialized (GBM / EGL 1.4 / GLES 2.0)\n");

    cherry_shell_register_symbol("mesa", (uintptr_t)cmd_mesa, CHERRY_SHELL_SYM_CMD, "Mesa 3D graphics commands: mesa [info|triangle|gears|test]");
    cherry_shell_register_symbol("es2gears", (uintptr_t)cmd_es2gears, CHERRY_SHELL_SYM_CMD, "Run classical 3D es2gears benchmark: es2gears [frames]");
    cherry_shell_register_symbol("bifrost", (uintptr_t)cmd_bifrost, CHERRY_SHELL_SYM_CMD, "Panfrost Bifrost 3D graphics commands: bifrost [info|triangle|gears|test]");

    s_inited = true;
    return 0;
}
