/*
 * CherryMesa - 3D Graphics Library
 *
 * Master Header for CherryMesa & Gallium3D Subsystem
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_MESA_MASTER_H_
#define _CHERRY_MESA_MASTER_H_

#include "gbm.h"
#include "EGL/egl.h"
#include "GLES2/gl2.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Gallium3D Driver Types */
typedef enum {
    GALLIUM_DRIVER_SOFTPERF = 0,
    GALLIUM_DRIVER_VIRGL    = 1,
    GALLIUM_DRIVER_PANFROST = 2,
} gallium_driver_type_t;

struct pipe_resource {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t stride;
    void    *vaddr;
    uint32_t handle;
};

struct pipe_screen {
    const char           *driver_name;
    gallium_driver_type_t driver_type;
    int                   drm_fd;

    struct pipe_resource *(*resource_create)(struct pipe_screen *ps, uint32_t w, uint32_t h, uint32_t format);
    void (*resource_destroy)(struct pipe_screen *ps, struct pipe_resource *res);
};

struct pipe_context {
    struct pipe_screen   *screen;
    struct pipe_resource *render_target;
    struct pipe_resource *depth_buffer;

    uint32_t              viewport_x;
    uint32_t              viewport_y;
    uint32_t              viewport_w;
    uint32_t              viewport_h;

    uint32_t              clear_color;
    float                 clear_depth;

    void (*clear)(struct pipe_context *ctx, uint32_t mask);
    void (*draw_triangles)(struct pipe_context *ctx, const float *vertices, int count);
    void (*flush)(struct pipe_context *ctx);
};

/* Core Subsystem APIs */
int  cherry_mesa_init(void);
int  cherry_mesa_run_tests(void);

/* Demos & Benchmarks */
int  cherry_mesa_run_gears(int frames);
int  cherry_mesa_run_triangle(int frames);
int  cherry_mesa_run_bifrost_triangle(int frames);
int  cherry_mesa_run_bifrost_gears(int frames);

/* Gallium Backend Constructors */
struct pipe_screen *pipe_screen_create(int drm_fd);
struct pipe_context *pipe_context_create(struct pipe_screen *screen);
void                pipe_context_destroy(struct pipe_context *ctx);
void                pipe_screen_destroy(struct pipe_screen *screen);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_MESA_MASTER_H_ */
