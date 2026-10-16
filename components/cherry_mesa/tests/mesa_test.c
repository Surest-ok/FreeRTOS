/*
 * CherryMesa - 3D Graphics Library Acceptance Tests
 *
 * Verifies GBM, EGL 1.4, OpenGL ES 2.0 & Gallium3D Pipeline
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_mesa.h"
#include "cherry_kal.h"
#include <string.h>

static bool test_gbm_bo_allocation(void)
{
    struct gbm_device *gbm = gbm_create_device(0);
    if (!gbm) return false;

    struct gbm_bo *bo = gbm_bo_create(gbm, 800, 600, GBM_FORMAT_XRGB8888, GBM_BO_USE_RENDERING);
    if (!bo) {
        gbm_device_destroy(gbm);
        return false;
    }

    if (gbm_bo_get_width(bo) != 800 || gbm_bo_get_height(bo) != 600) {
        gbm_bo_destroy(bo);
        gbm_device_destroy(gbm);
        return false;
    }

    if (gbm_bo_get_format(bo) != GBM_FORMAT_XRGB8888 || gbm_bo_get_stride(bo) < 800 * 4) {
        gbm_bo_destroy(bo);
        gbm_device_destroy(gbm);
        return false;
    }

    uint32_t stride = 0;
    void *map = NULL;
    void *vaddr = gbm_bo_map(bo, 0, 0, 800, 600, 0, &stride, &map);
    if (!vaddr) {
        gbm_bo_destroy(bo);
        gbm_device_destroy(gbm);
        return false;
    }

    /* Verify memory access */
    uint32_t *pixels = (uint32_t *)vaddr;
    pixels[0] = 0xAA55CC33;
    pixels[799] = 0x12345678;
    if (pixels[0] != 0xAA55CC33 || pixels[799] != 0x12345678) {
        gbm_bo_destroy(bo);
        gbm_device_destroy(gbm);
        return false;
    }

    gbm_bo_unmap(bo, map);

    /* Verify PRIME DMA-BUF handle export */
    int prime_fd = gbm_bo_get_fd(bo);
    if (prime_fd <= 0) {
        gbm_bo_destroy(bo);
        gbm_device_destroy(gbm);
        return false;
    }

    gbm_bo_destroy(bo);
    gbm_device_destroy(gbm);
    return true;
}

static bool test_gbm_surface(void)
{
    struct gbm_device *gbm = gbm_create_device(0);
    if (!gbm) return false;

    struct gbm_surface *surf = gbm_surface_create(gbm, 320, 240, GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT);
    if (!surf) {
        gbm_device_destroy(gbm);
        return false;
    }

    struct gbm_bo *front = gbm_surface_lock_front_buffer(surf);
    if (!front || gbm_bo_get_width(front) != 320 || gbm_bo_get_height(front) != 240) {
        gbm_surface_destroy(surf);
        gbm_device_destroy(gbm);
        return false;
    }

    struct gbm_bo *back = gbm_surface_lock_front_buffer(surf);
    if (!back || back == front) {
        gbm_surface_destroy(surf);
        gbm_device_destroy(gbm);
        return false;
    }

    gbm_surface_release_buffer(surf, front);
    gbm_surface_release_buffer(surf, back);
    gbm_surface_destroy(surf);
    gbm_device_destroy(gbm);
    return true;
}

static bool test_egl_lifecycle(void)
{
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY) return false;

    EGLint major = 0, minor = 0;
    if (!eglInitialize(dpy, &major, &minor)) return false;
    if (major != 1 || minor != 4) return false;

    const char *ver = eglQueryString(dpy, EGL_VERSION);
    const char *vendor = eglQueryString(dpy, EGL_VENDOR);
    const char *apis = eglQueryString(dpy, EGL_CLIENT_APIS);
    if (!ver || !vendor || !apis) return false;

    EGLConfig configs[2];
    EGLint num_configs = 0;
    if (!eglChooseConfig(dpy, NULL, configs, 2, &num_configs) || num_configs < 1) return false;

    EGLint r = 0, g = 0, b = 0, d = 0;
    eglGetConfigAttrib(dpy, configs[0], EGL_RED_SIZE, &r);
    eglGetConfigAttrib(dpy, configs[0], EGL_GREEN_SIZE, &g);
    eglGetConfigAttrib(dpy, configs[0], EGL_BLUE_SIZE, &b);
    eglGetConfigAttrib(dpy, configs[0], EGL_DEPTH_SIZE, &d);
    if (r != 8 || g != 8 || b != 8 || d != 24) return false;

    EGLContext ctx = eglCreateContext(dpy, configs[0], EGL_NO_CONTEXT, NULL);
    if (ctx == EGL_NO_CONTEXT) return false;

    EGLSurface surf = eglCreateWindowSurface(dpy, configs[0], NULL, NULL);
    if (surf == EGL_NO_SURFACE) {
        eglDestroyContext(dpy, ctx);
        return false;
    }

    if (!eglMakeCurrent(dpy, surf, surf, ctx)) {
        eglDestroySurface(dpy, surf);
        eglDestroyContext(dpy, ctx);
        return false;
    }

    if (!eglSwapBuffers(dpy, surf)) {
        eglDestroySurface(dpy, surf);
        eglDestroyContext(dpy, ctx);
        return false;
    }

    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(dpy, surf);
    eglDestroyContext(dpy, ctx);
    eglTerminate(dpy);
    return true;
}

static bool test_gles2_pipeline(void)
{
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(dpy, NULL, NULL)) return false;

    EGLConfig config = NULL;
    EGLint num_config = 0;
    eglChooseConfig(dpy, NULL, &config, 1, &num_config);

    EGLContext ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, NULL);
    EGLSurface surf = eglCreateWindowSurface(dpy, config, NULL, NULL);
    eglMakeCurrent(dpy, surf, surf, ctx);

    const GLubyte *renderer = glGetString(GL_RENDERER);
    const GLubyte *gl_ver = glGetString(GL_VERSION);
    if (!renderer || !gl_ver) return false;

    /* Compile Shaders */
    const char *vs_code = "attribute vec3 a_Pos; void main() { gl_Position = vec4(a_Pos, 1.0); }";
    const char *fs_code = "precision mediump float; void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); }";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_code, NULL);
    glCompileShader(vs);

    GLint compile_status = GL_FALSE;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &compile_status);
    if (compile_status != GL_TRUE) return false;

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_code, NULL);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &compile_status);
    if (compile_status != GL_TRUE) return false;

    /* Link Program */
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint link_status = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &link_status);
    if (link_status != GL_TRUE) return false;

    glUseProgram(prog);

    /* Allocate VBO */
    float verts[9] = {
         0.0f,  0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
    };
    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    /* Draw and Swap */
    glViewport(0, 0, 640, 480);
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    eglSwapBuffers(dpy, surf);

    /* Clean up */
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    eglDestroySurface(dpy, surf);
    eglDestroyContext(dpy, ctx);
    eglTerminate(dpy);
    return true;
}

static bool test_gallium_drivers(void)
{
    struct pipe_screen *screen = pipe_screen_create(0);
    if (!screen || !screen->driver_name) return false;

    if (screen->driver_type != GALLIUM_DRIVER_PANFROST &&
        screen->driver_type != GALLIUM_DRIVER_VIRGL &&
        screen->driver_type != GALLIUM_DRIVER_SOFTPERF) {
        return false;
    }

    struct pipe_context *ctx = pipe_context_create(screen);
    if (!ctx) return false;

    struct pipe_resource *rt = screen->resource_create(screen, 64, 64, GBM_FORMAT_XRGB8888);
    if (!rt || !rt->vaddr) {
        kfree(ctx);
        kfree(screen);
        return false;
    }

    ctx->render_target = rt;
    ctx->viewport_w = 64;
    ctx->viewport_h = 64;
    ctx->clear_color = 0xFF00FF00; /* Green */
    ctx->clear(ctx, GL_COLOR_BUFFER_BIT);

    uint32_t *buf = (uint32_t *)rt->vaddr;
    if (buf[0] != 0xFF00FF00 || buf[64 * 63 + 63] != 0xFF00FF00) {
        screen->resource_destroy(screen, rt);
        kfree(ctx);
        kfree(screen);
        return false;
    }

    /* Draw test triangle */
    float tri[18] = {
         0.0f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,
         0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,
    };
    ctx->draw_triangles(ctx, tri, 3);
    ctx->flush(ctx);

    screen->resource_destroy(screen, rt);
    pipe_context_destroy(ctx);
    pipe_screen_destroy(screen);
    return true;
}

static bool test_demos_execution(void)
{
    int ret1 = cherry_mesa_run_triangle(2);
    if (ret1 != 0) return false;

    int ret2 = cherry_mesa_run_gears(2);
    if (ret2 != 0) return false;

    return true;
}


static bool test_bifrost_pipeline(void)
{
    int ret1 = cherry_mesa_run_bifrost_triangle(2);
    if (ret1 != 0) return false;

    int ret2 = cherry_mesa_run_bifrost_gears(2);
    if (ret2 != 0) return false;

    return true;
}

int cherry_mesa_run_tests(void)
{
    pr_info("cherry_mesa: starting acceptance test suite...\n");

    bool gbm_pass     = test_gbm_bo_allocation();
    bool surf_pass    = test_gbm_surface();
    bool egl_pass     = test_egl_lifecycle();
    bool gles2_pass   = test_gles2_pipeline();
    bool gallium_pass = test_gallium_drivers();
    bool demos_pass   = test_demos_execution();

    bool bifrost_pass = test_bifrost_pipeline();

    bool all_pass = gbm_pass && surf_pass && egl_pass &&
                    gles2_pass && gallium_pass && demos_pass && bifrost_pass;

    printk("cherry_mesa: [mesa-verify] gbm=%s surface=%s egl=%s gles2=%s gallium=%s demos=%s bifrost=%s all=%s\n",
           gbm_pass ? "pass" : "FAIL",
           surf_pass ? "pass" : "FAIL",
           egl_pass ? "pass" : "FAIL",
           gles2_pass ? "pass" : "FAIL",
           gallium_pass ? "pass" : "FAIL",
           demos_pass ? "pass" : "FAIL",
           bifrost_pass ? "pass" : "FAIL",
           all_pass ? "pass" : "FAIL");

    return all_pass ? 0 : -1;
}
