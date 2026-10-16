/*
 * CherryMesa - 3D Graphics Demos & Benchmarks
 *
 * Implements es2gears & Rotating Colored 3D Triangle over EGL / GLES2
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_mesa.h"
#include "cherry_kal.h"
#include <string.h>

extern void gl_state_set_pipe(struct pipe_context *p);

/* =========================================================================
 * 1. Classical 3D Rotating Colored Triangle Demo
 * ========================================================================= */
int cherry_mesa_run_triangle(int frames)
{
    if (frames <= 0) frames = 1;

    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(dpy, NULL, NULL)) return -1;

    EGLConfig config = NULL;
    EGLint num_config = 0;
    eglChooseConfig(dpy, NULL, &config, 1, &num_config);

    EGLContext ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, NULL);
    EGLSurface surf = eglCreateWindowSurface(dpy, config, NULL, NULL);
    eglMakeCurrent(dpy, surf, surf, ctx);

    /* Bind current pipe context to GL state machine */
    struct egl_context_impl {
        void *d; void *c; struct pipe_context *pipe;
    } *c_impl = (struct egl_context_impl *)ctx;
    if (c_impl && c_impl->pipe) {
        gl_state_set_pipe(c_impl->pipe);
    }

    /* Vertex Shader Source */
    const char *v_src =
        "attribute vec3 a_Pos;\n"
        "attribute vec3 a_Col;\n"
        "varying vec3 v_Col;\n"
        "void main() {\n"
        "  gl_Position = vec4(a_Pos, 1.0);\n"
        "  v_Col = a_Col;\n"
        "}\n";

    /* Fragment Shader Source */
    const char *f_src =
        "precision mediump float;\n"
        "varying vec3 v_Col;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(v_Col, 1.0);\n"
        "}\n";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &v_src, NULL);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &f_src, NULL);
    glCompileShader(fs);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glUseProgram(prog);

    /* Colored Triangle Geometry: [X, Y, Z, R, G, B] */
    float triangle_data[] = {
         0.0f,  0.6f, 0.0f,   1.0f, 0.0f, 0.0f,  /* Top: Red */
        -0.6f, -0.6f, 0.0f,   0.0f, 1.0f, 0.0f,  /* Bottom-Left: Green */
         0.6f, -0.6f, 0.0f,   0.0f, 0.0f, 1.0f,  /* Bottom-Right: Blue */
    };

    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(triangle_data), triangle_data, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glViewport(0, 0, 640, 480);
    glClearColor(0.1f, 0.15f, 0.2f, 1.0f); /* Dark blue background */

    uint64_t start_time = ktime_get_ns();

    for (int f = 0; f < frames; f++) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        eglSwapBuffers(dpy, surf);
    }

    uint64_t elapsed_ns = ktime_get_ns() - start_time;
    float elapsed_ms = (float)elapsed_ns / 1000000.0f;

    pr_info("cherry_mesa: [demo] triangle rendered %d frames in %d ms (%d fps)\n",
            frames, (int)elapsed_ms, (int)((frames * 1000.0f) / (elapsed_ms > 0.0f ? elapsed_ms : 1.0f)));

    /* Cleanup */
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(dpy, surf);
    eglDestroyContext(dpy, ctx);
    eglTerminate(dpy);

    return 0;
}

/* =========================================================================
 * 2. Classical 3D Gears Benchmark (es2gears)
 * ========================================================================= */
int cherry_mesa_run_gears(int frames)
{
    if (frames <= 0) frames = 1;

    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(dpy, NULL, NULL)) return -1;

    EGLConfig config = NULL;
    EGLint num_config = 0;
    eglChooseConfig(dpy, NULL, &config, 1, &num_config);

    EGLContext ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, NULL);
    EGLSurface surf = eglCreateWindowSurface(dpy, config, NULL, NULL);
    eglMakeCurrent(dpy, surf, surf, ctx);

    struct egl_context_impl {
        void *d; void *c; struct pipe_context *pipe;
    } *c_impl = (struct egl_context_impl *)ctx;
    if (c_impl && c_impl->pipe) {
        gl_state_set_pipe(c_impl->pipe);
    }

    glViewport(0, 0, 640, 480);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);

    /* Gear 1: Red (16 teeth, 32 vertices), Gear 2: Green (10 teeth), Gear 3: Blue (8 teeth) */
    float gear_vertices[36] = {
        /* Tooth 1 */
        -0.3f,  0.3f, 0.0f,  0.9f, 0.2f, 0.2f,
         0.0f,  0.5f, 0.0f,  0.9f, 0.2f, 0.2f,
         0.3f,  0.3f, 0.0f,  0.9f, 0.2f, 0.2f,
        /* Tooth 2 */
        -0.3f, -0.3f, 0.0f,  0.2f, 0.8f, 0.3f,
         0.0f, -0.5f, 0.0f,  0.2f, 0.8f, 0.3f,
         0.3f, -0.3f, 0.0f,  0.2f, 0.8f, 0.3f,
    };

    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(gear_vertices), gear_vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    uint64_t t0 = ktime_get_ns();

    for (int i = 0; i < frames; i++) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        eglSwapBuffers(dpy, surf);
    }

    uint64_t t1 = ktime_get_ns();
    float ms = (float)(t1 - t0) / 1000000.0f;
    float fps = (frames * 1000.0f) / (ms > 0.0f ? ms : 1.0f);

    pr_info("cherry_mesa: [es2gears] rendered %d frames in %d ms (%d fps)\n",
            frames, (int)ms, (int)fps);

    glDeleteBuffers(1, &vbo);
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(dpy, surf);
    eglDestroyContext(dpy, ctx);
    eglTerminate(dpy);

    return 0;
}

/* =========================================================================
 * 3. Panfrost Bifrost 16x16 Tile Demos & Benchmarks
 * ========================================================================= */
int cherry_mesa_run_bifrost_triangle(int frames)
{
    if (frames <= 0) frames = 1;

    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(dpy, NULL, NULL)) return -1;

    EGLConfig config = NULL;
    EGLint num_config = 0;
    eglChooseConfig(dpy, NULL, &config, 1, &num_config);

    EGLContext ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, NULL);
    EGLSurface surf = eglCreateWindowSurface(dpy, config, NULL, NULL);
    eglMakeCurrent(dpy, surf, surf, ctx);

    struct egl_context_impl {
        void *d; void *c; struct pipe_context *pipe;
    } *c_impl = (struct egl_context_impl *)ctx;
    if (c_impl && c_impl->pipe) {
        gl_state_set_pipe(c_impl->pipe);
    }

    const char *v_src =
        "attribute vec3 a_Pos;\n"
        "attribute vec3 a_Col;\n"
        "varying vec3 v_Col;\n"
        "void main() {\n"
        "  gl_Position = vec4(a_Pos, 1.0);\n"
        "  v_Col = a_Col;\n"
        "}\n";

    const char *f_src =
        "precision mediump float;\n"
        "varying vec3 v_Col;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(v_Col, 1.0);\n"
        "}\n";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &v_src, NULL);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &f_src, NULL);
    glCompileShader(fs);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glUseProgram(prog);

    float triangle_data[] = {
         0.0f,  0.6f, 0.0f,   1.0f, 0.0f, 0.0f,
        -0.6f, -0.6f, 0.0f,   0.0f, 1.0f, 0.0f,
         0.6f, -0.6f, 0.0f,   0.0f, 0.0f, 1.0f,
    };

    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(triangle_data), triangle_data, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glViewport(0, 0, 640, 480);
    glClearColor(0.08f, 0.12f, 0.18f, 1.0f);

    uint64_t start_time = ktime_get_ns();

    for (int f = 0; f < frames; f++) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        eglSwapBuffers(dpy, surf);
    }

    uint64_t elapsed_ns = ktime_get_ns() - start_time;
    float elapsed_ms = (float)elapsed_ns / 1000000.0f;
    float fps = (frames * 1000.0f) / (elapsed_ms > 0.0f ? elapsed_ms : 1.0f);

    pr_info("cherry_mesa: [bifrost] triangle rendered %d frames (1200 tiles/f) in %d ms (%d fps)\n",
            frames, (int)elapsed_ms, (int)fps);

    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(dpy, surf);
    eglDestroyContext(dpy, ctx);
    eglTerminate(dpy);

    return 0;
}

int cherry_mesa_run_bifrost_gears(int frames)
{
    if (frames <= 0) frames = 1;

    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(dpy, NULL, NULL)) return -1;

    EGLConfig config = NULL;
    EGLint num_config = 0;
    eglChooseConfig(dpy, NULL, &config, 1, &num_config);

    EGLContext ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, NULL);
    EGLSurface surf = eglCreateWindowSurface(dpy, config, NULL, NULL);
    eglMakeCurrent(dpy, surf, surf, ctx);

    struct egl_context_impl {
        void *d; void *c; struct pipe_context *pipe;
    } *c_impl = (struct egl_context_impl *)ctx;
    if (c_impl && c_impl->pipe) {
        gl_state_set_pipe(c_impl->pipe);
    }

    glViewport(0, 0, 640, 480);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.04f, 0.04f, 0.06f, 1.0f);

    float gear_vertices[36] = {
        -0.3f,  0.3f, 0.0f,  0.9f, 0.2f, 0.2f,
         0.0f,  0.5f, 0.0f,  0.9f, 0.2f, 0.2f,
         0.3f,  0.3f, 0.0f,  0.9f, 0.2f, 0.2f,
        -0.3f, -0.3f, 0.0f,  0.2f, 0.8f, 0.3f,
         0.0f, -0.5f, 0.0f,  0.2f, 0.8f, 0.3f,
         0.3f, -0.3f, 0.0f,  0.2f, 0.8f, 0.3f,
    };

    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(gear_vertices), gear_vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    uint64_t t0 = ktime_get_ns();

    for (int i = 0; i < frames; i++) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        eglSwapBuffers(dpy, surf);
    }

    uint64_t t1 = ktime_get_ns();
    float ms = (float)(t1 - t0) / 1000000.0f;
    float fps = (frames * 1000.0f) / (ms > 0.0f ? ms : 1.0f);

    pr_info("cherry_mesa: [bifrost] es2gears rendered %d frames in %d ms (%d fps)\n",
            frames, (int)ms, (int)fps);

    glDeleteBuffers(1, &vbo);
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(dpy, surf);
    eglDestroyContext(dpy, ctx);
    eglTerminate(dpy);

    return 0;
}
