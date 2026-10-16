/*
 * CherryMesa - Embedded Graphics Library (EGL 1.4)
 *
 * Implements EGL Context, Configs, Displays & Surface Presentation
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "EGL/egl.h"
#include "cherry_mesa.h"
#include "cherry_drm.h"
#include "cherry_kal.h"
#include <string.h>

struct egl_display_impl {
    bool                initialized;
    struct gbm_device  *gbm;
    struct pipe_screen *screen;
};

struct egl_config_impl {
    EGLint              config_id;
    EGLint              red_size;
    EGLint              green_size;
    EGLint              blue_size;
    EGLint              alpha_size;
    EGLint              depth_size;
    EGLint              stencil_size;
    EGLint              renderable_type;
    EGLint              surface_type;
};

struct egl_surface_impl {
    struct egl_display_impl *dpy;
    struct egl_config_impl  *cfg;
    struct gbm_surface      *gbm_surf;
    struct pipe_resource    *color_buffers[2]; /* Front and Back */
    struct pipe_resource    *depth_buffer;
    int                      current_buf;
    uint32_t                 width;
    uint32_t                 height;
    bool                     is_pbuffer;
};

struct egl_context_impl {
    struct egl_display_impl *dpy;
    struct egl_config_impl  *cfg;
    struct pipe_context     *pipe;
    EGLint                   client_version;
};

static struct egl_display_impl s_egl_dpy;
static struct egl_config_impl  s_egl_configs[2] = {
    {
        .config_id = 1,
        .red_size = 8, .green_size = 8, .blue_size = 8, .alpha_size = 8,
        .depth_size = 24, .stencil_size = 8,
        .renderable_type = EGL_OPENGL_ES2_BIT,
        .surface_type = EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
    },
    {
        .config_id = 2,
        .red_size = 5, .green_size = 6, .blue_size = 5, .alpha_size = 0,
        .depth_size = 16, .stencil_size = 0,
        .renderable_type = EGL_OPENGL_ES2_BIT,
        .surface_type = EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
    }
};

static struct egl_context_impl *s_current_context = NULL;
static struct egl_surface_impl *s_current_surface = NULL;
static EGLint s_last_error = EGL_SUCCESS;

EGLAPI EGLDisplay EGLAPIENTRY eglGetDisplay(EGLNativeDisplayType display_id)
{
    (void)display_id;
    return (EGLDisplay)&s_egl_dpy;
}

EGLAPI EGLBoolean EGLAPIENTRY eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor)
{
    if (dpy != (EGLDisplay)&s_egl_dpy) {
        s_last_error = EGL_BAD_DISPLAY;
        return EGL_FALSE;
    }

    struct egl_display_impl *d = (struct egl_display_impl *)dpy;
    if (!d->initialized) {
        d->gbm = gbm_create_device(0);
        d->screen = pipe_screen_create(0);
        d->initialized = true;
    }

    if (major) *major = 1;
    if (minor) *minor = 4;
    return EGL_TRUE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglTerminate(EGLDisplay dpy)
{
    if (dpy != (EGLDisplay)&s_egl_dpy) return EGL_FALSE;
    if (s_egl_dpy.gbm) {
        gbm_device_destroy(s_egl_dpy.gbm);
        s_egl_dpy.gbm = NULL;
    }
    if (s_egl_dpy.screen) {
        pipe_screen_destroy(s_egl_dpy.screen);
        s_egl_dpy.screen = NULL;
    }
    s_egl_dpy.initialized = false;
    return EGL_TRUE;
}

EGLAPI const char *EGLAPIENTRY eglQueryString(EGLDisplay dpy, EGLint name)
{
    (void)dpy;
    switch (name) {
    case EGL_VENDOR:
        return "CherryOS / Mesa Project";
    case EGL_VERSION:
        return "1.4 CherryMesa 24.0.0";
    case EGL_EXTENSIONS:
        return "EGL_KHR_image_base EGL_KHR_gl_texture_2D_image EGL_KHR_surfaceless_context";
    case EGL_CLIENT_APIS:
        return "OpenGL_ES";
    default:
        return NULL;
    }
}

EGLAPI EGLint EGLAPIENTRY eglGetError(void)
{
    EGLint err = s_last_error;
    s_last_error = EGL_SUCCESS;
    return err;
}

EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigs(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
    (void)dpy;
    if (!num_config) return EGL_FALSE;
    *num_config = 2;
    if (configs && config_size >= 2) {
        configs[0] = (EGLConfig)&s_egl_configs[0];
        configs[1] = (EGLConfig)&s_egl_configs[1];
    }
    return EGL_TRUE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
    (void)dpy; (void)attrib_list;
    return eglGetConfigs(dpy, configs, config_size, num_config);
}

EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value)
{
    (void)dpy;
    if (!config || !value) return EGL_FALSE;
    struct egl_config_impl *c = (struct egl_config_impl *)config;

    switch (attribute) {
    case EGL_CONFIG_ID:       *value = c->config_id; break;
    case EGL_RED_SIZE:        *value = c->red_size; break;
    case EGL_GREEN_SIZE:      *value = c->green_size; break;
    case EGL_BLUE_SIZE:       *value = c->blue_size; break;
    case EGL_ALPHA_SIZE:      *value = c->alpha_size; break;
    case EGL_DEPTH_SIZE:      *value = c->depth_size; break;
    case EGL_STENCIL_SIZE:    *value = c->stencil_size; break;
    case EGL_RENDERABLE_TYPE: *value = c->renderable_type; break;
    case EGL_SURFACE_TYPE:    *value = c->surface_type; break;
    default:                  *value = 0; break;
    }
    return EGL_TRUE;
}

EGLAPI EGLContext EGLAPIENTRY eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list)
{
    (void)share_context;
    if (dpy != (EGLDisplay)&s_egl_dpy) return EGL_NO_CONTEXT;

    struct egl_context_impl *ctx = (struct egl_context_impl *)kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx) return EGL_NO_CONTEXT;

    ctx->dpy = (struct egl_display_impl *)dpy;
    ctx->cfg = (struct egl_config_impl *)config;
    ctx->client_version = 2; /* GLES2 */

    if (attrib_list) {
        for (int i = 0; attrib_list[i] != EGL_NONE; i += 2) {
            if (attrib_list[i] == EGL_CONTEXT_CLIENT_VERSION) {
                ctx->client_version = attrib_list[i + 1];
            }
        }
    }

    ctx->pipe = pipe_context_create(ctx->dpy->screen);
    return (EGLContext)ctx;
}

EGLAPI EGLBoolean EGLAPIENTRY eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
    (void)dpy;
    if (!ctx) return EGL_FALSE;
    struct egl_context_impl *c = (struct egl_context_impl *)ctx;
    if (s_current_context == c) s_current_context = NULL;
    if (c->pipe) {
        pipe_context_destroy(c->pipe);
        c->pipe = NULL;
    }
    kfree(c);
    return EGL_TRUE;
}

EGLAPI EGLSurface EGLAPIENTRY eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list)
{
    (void)attrib_list;
    if (dpy != (EGLDisplay)&s_egl_dpy) return EGL_NO_SURFACE;

    struct egl_surface_impl *surf = (struct egl_surface_impl *)kzalloc(sizeof(*surf), GFP_KERNEL);
    if (!surf) return EGL_NO_SURFACE;

    surf->dpy = (struct egl_display_impl *)dpy;
    surf->cfg = (struct egl_config_impl *)config;

    if (win) {
        struct gbm_surface *gbm_surf = (struct gbm_surface *)win;
        surf->gbm_surf = gbm_surf;
        surf->width = gbm_surface_get_width(gbm_surf);
        surf->height = gbm_surface_get_height(gbm_surf);
    } else {
        surf->width = 640;
        surf->height = 480;
    }

    /* Create Pipe Resources */
    surf->color_buffers[0] = surf->dpy->screen->resource_create(surf->dpy->screen, surf->width, surf->height, GBM_FORMAT_XRGB8888);
    surf->color_buffers[1] = surf->dpy->screen->resource_create(surf->dpy->screen, surf->width, surf->height, GBM_FORMAT_XRGB8888);
    surf->depth_buffer     = surf->dpy->screen->resource_create(surf->dpy->screen, surf->width, surf->height, 0); /* Depth buffer */
    surf->current_buf = 0;

    if (!surf->color_buffers[0] || !surf->color_buffers[1] || !surf->depth_buffer) {
        if (surf->color_buffers[0]) surf->dpy->screen->resource_destroy(surf->dpy->screen, surf->color_buffers[0]);
        if (surf->color_buffers[1]) surf->dpy->screen->resource_destroy(surf->dpy->screen, surf->color_buffers[1]);
        if (surf->depth_buffer)     surf->dpy->screen->resource_destroy(surf->dpy->screen, surf->depth_buffer);
        kfree(surf);
        return EGL_NO_SURFACE;
    }

    return (EGLSurface)surf;
}

EGLAPI EGLSurface EGLAPIENTRY eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list)
{
    if (dpy != (EGLDisplay)&s_egl_dpy) return EGL_NO_SURFACE;

    struct egl_surface_impl *surf = (struct egl_surface_impl *)kzalloc(sizeof(*surf), GFP_KERNEL);
    if (!surf) return EGL_NO_SURFACE;

    surf->dpy = (struct egl_display_impl *)dpy;
    surf->cfg = (struct egl_config_impl *)config;
    surf->width = 320;
    surf->height = 240;
    surf->is_pbuffer = true;

    if (attrib_list) {
        for (int i = 0; attrib_list[i] != EGL_NONE; i += 2) {
            if (attrib_list[i] == EGL_WIDTH) surf->width = attrib_list[i + 1];
            if (attrib_list[i] == EGL_HEIGHT) surf->height = attrib_list[i + 1];
        }
    }

    surf->color_buffers[0] = surf->dpy->screen->resource_create(surf->dpy->screen, surf->width, surf->height, GBM_FORMAT_XRGB8888);
    surf->depth_buffer     = surf->dpy->screen->resource_create(surf->dpy->screen, surf->width, surf->height, 0);
    return (EGLSurface)surf;
}

EGLAPI EGLBoolean EGLAPIENTRY eglDestroySurface(EGLDisplay dpy, EGLSurface surface)
{
    (void)dpy;
    if (!surface) return EGL_FALSE;
    struct egl_surface_impl *s = (struct egl_surface_impl *)surface;
    if (s_current_surface == s) s_current_surface = NULL;

    if (s->dpy && s->dpy->screen) {
        if (s->color_buffers[0]) s->dpy->screen->resource_destroy(s->dpy->screen, s->color_buffers[0]);
        if (s->color_buffers[1]) s->dpy->screen->resource_destroy(s->dpy->screen, s->color_buffers[1]);
        if (s->depth_buffer)     s->dpy->screen->resource_destroy(s->dpy->screen, s->depth_buffer);
    }
    kfree(s);
    return EGL_TRUE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
{
    (void)dpy; (void)read;
    if (!ctx || !draw) {
        s_current_context = NULL;
        s_current_surface = NULL;
        return EGL_TRUE;
    }

    struct egl_context_impl *c = (struct egl_context_impl *)ctx;
    struct egl_surface_impl *s = (struct egl_surface_impl *)draw;

    s_current_context = c;
    s_current_surface = s;

    /* Bind render target and depth buffer into Gallium pipe context */
    if (c->pipe) {
        c->pipe->render_target = s->color_buffers[s->current_buf];
        c->pipe->depth_buffer = s->depth_buffer;
        c->pipe->viewport_w = s->width;
        c->pipe->viewport_h = s->height;
    }
    return EGL_TRUE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
    (void)dpy;
    if (!surface) return EGL_FALSE;
    struct egl_surface_impl *s = (struct egl_surface_impl *)surface;

    /* 1. Flush Gallium 3D drawing commands */
    if (s_current_context && s_current_context->pipe && s_current_context->pipe->flush) {
        s_current_context->pipe->flush(s_current_context->pipe);
    }

    /* 2. Copy back buffer pixels to display scanout / front buffer */
    struct drm_device *card = cherry_drm_get_primary();
    if (card && card->crtc.fb && card->crtc.fb->objs[0] && s->color_buffers[s->current_buf]) {
        void *fb_vaddr = card->crtc.fb->objs[0]->vaddr;
        void *src_vaddr = s->color_buffers[s->current_buf]->vaddr;
        if (fb_vaddr && src_vaddr) {
            size_t max_fb = card->crtc.fb->objs[0]->size;
            size_t sz = (size_t)s->width * s->height * 4;
            if (sz > max_fb) sz = max_fb;
            memcpy(fb_vaddr, src_vaddr, sz);
        }

        /* 3. Notify DRM of dirty region / page flip */
        struct drm_mode_fb_dirty_cmd dirty;
        memset(&dirty, 0, sizeof(dirty));
        dirty.fb_id = card->crtc.fb_id;
        cherry_drm_ioctl(card, DRM_IOCTL_MODE_DIRTYFB, &dirty);
    }

    /* 4. Swap buffer index */
    s->current_buf = 1 - s->current_buf;
    if (s_current_context && s_current_context->pipe) {
        s_current_context->pipe->render_target = s->color_buffers[s->current_buf];
    }

    return EGL_TRUE;
}

EGLAPI void *EGLAPIENTRY eglGetProcAddress(const char *procname)
{
    (void)procname;
    return NULL;
}
