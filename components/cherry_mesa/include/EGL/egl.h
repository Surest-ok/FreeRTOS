/*
 * CherryMesa - Embedded Graphics Library (EGL 1.4)
 *
 * Upstream Mesa Custom-Tailored EGL API
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_EGL_H_
#define _CHERRY_EGL_H_

#include "eglplatform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;
typedef intptr_t     EGLint;
typedef void        *EGLDisplay;
typedef void        *EGLConfig;
typedef void        *EGLContext;
typedef void        *EGLSurface;

#define EGL_FALSE                       0
#define EGL_TRUE                        1

#define EGL_DEFAULT_DISPLAY             ((EGLNativeDisplayType)0)
#define EGL_NO_CONTEXT                  ((EGLContext)0)
#define EGL_NO_DISPLAY                  ((EGLDisplay)0)
#define EGL_NO_SURFACE                  ((EGLSurface)0)

#define EGL_DONT_CARE                   ((EGLint)-1)

/* Errors */
#define EGL_SUCCESS                     0x3000
#define EGL_NOT_INITIALIZED             0x3001
#define EGL_BAD_ACCESS                  0x3002
#define EGL_BAD_ALLOC                   0x3003
#define EGL_BAD_ATTRIBUTE               0x3004
#define EGL_BAD_CONFIG                  0x3005
#define EGL_BAD_CONTEXT                 0x3006
#define EGL_BAD_CURRENT_SURFACE         0x3007
#define EGL_BAD_DISPLAY                 0x3008
#define EGL_BAD_MATCH                   0x3009
#define EGL_BAD_NATIVE_PIXMAP           0x300A
#define EGL_BAD_NATIVE_WINDOW           0x300B
#define EGL_BAD_PARAMETER               0x300C
#define EGL_BAD_SURFACE                 0x300D
#define EGL_CONTEXT_LOST                0x300E

/* Config Attributes */
#define EGL_BUFFER_SIZE                 0x3020
#define EGL_ALPHA_SIZE                  0x3021
#define EGL_BLUE_SIZE                   0x3022
#define EGL_GREEN_SIZE                  0x3023
#define EGL_RED_SIZE                    0x3024
#define EGL_DEPTH_SIZE                  0x3025
#define EGL_STENCIL_SIZE                0x3026
#define EGL_CONFIG_CAVEAT               0x3027
#define EGL_CONFIG_ID                   0x3028
#define EGL_LEVEL                       0x3029
#define EGL_MAX_PBUFFER_HEIGHT          0x302A
#define EGL_MAX_PBUFFER_PIXELS          0x302B
#define EGL_MAX_PBUFFER_WIDTH           0x302C
#define EGL_NATIVE_RENDERABLE           0x302D
#define EGL_NATIVE_VISUAL_ID            0x302E
#define EGL_NATIVE_VISUAL_TYPE          0x302F
#define EGL_SAMPLES                     0x3031
#define EGL_SAMPLE_BUFFERS              0x3032
#define EGL_SURFACE_TYPE                0x3033
#define EGL_TRANSPARENT_TYPE            0x3034
#define EGL_NONE                        0x3038
#define EGL_RENDERABLE_TYPE             0x3040
#define EGL_HEIGHT                      0x3056
#define EGL_WIDTH                       0x3057

/* Surface Types */
#define EGL_PBUFFER_BIT                 0x0001
#define EGL_PIXMAP_BIT                  0x0002
#define EGL_WINDOW_BIT                  0x0004

/* Renderable Types */
#define EGL_OPENGL_ES_BIT               0x0001
#define EGL_OPENVG_BIT                  0x0002
#define EGL_OPENGL_ES2_BIT              0x0004
#define EGL_OPENGL_BIT                  0x0008

/* Context Attributes */
#define EGL_CONTEXT_CLIENT_VERSION      0x3098

/* String Queries */
#define EGL_VENDOR                      0x3053
#define EGL_VERSION                     0x3054
#define EGL_EXTENSIONS                  0x3055
#define EGL_CLIENT_APIS                 0x308D

/* Functions */
EGLAPI EGLDisplay EGLAPIENTRY eglGetDisplay(EGLNativeDisplayType display_id);
EGLAPI EGLBoolean EGLAPIENTRY eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor);
EGLAPI EGLBoolean EGLAPIENTRY eglTerminate(EGLDisplay dpy);

EGLAPI const char *EGLAPIENTRY eglQueryString(EGLDisplay dpy, EGLint name);
EGLAPI EGLint     EGLAPIENTRY eglGetError(void);

EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigs(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config);
EGLAPI EGLBoolean EGLAPIENTRY eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);
EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value);

EGLAPI EGLContext EGLAPIENTRY eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list);
EGLAPI EGLBoolean EGLAPIENTRY eglDestroyContext(EGLDisplay dpy, EGLContext ctx);
EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);

EGLAPI EGLSurface EGLAPIENTRY eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list);
EGLAPI EGLSurface EGLAPIENTRY eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list);
EGLAPI EGLBoolean EGLAPIENTRY eglDestroySurface(EGLDisplay dpy, EGLSurface surface);

EGLAPI EGLBoolean EGLAPIENTRY eglSwapBuffers(EGLDisplay dpy, EGLSurface surface);
EGLAPI void      *EGLAPIENTRY eglGetProcAddress(const char *procname);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_EGL_H_ */
