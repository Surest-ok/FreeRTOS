/*
 * CherryMesa - OpenGL ES 2.0 API State Tracker
 *
 * Implements Shaders, VBOs, State Machine & Triangle Drawing Pipeline
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "GLES2/gl2.h"
#include "cherry_mesa.h"
#include "cherry_kal.h"
#include <string.h>

#define MAX_GL_BUFFERS          32
#define MAX_GL_SHADERS          16
#define MAX_GL_PROGRAMS         8
#define MAX_VERTEX_ATTRIBS      8

struct gl_buffer_obj {
    GLuint  id;
    GLenum  target;
    GLenum  usage;
    size_t  size;
    void   *data;
};

struct gl_shader_obj {
    GLuint  id;
    GLenum  type;
    bool    compiled;
    char   *source;
};

struct gl_program_obj {
    GLuint  id;
    bool    linked;
    GLuint  vert_shader;
    GLuint  frag_shader;
    float   mvp[16];
    float   uniform_color[4];
};

struct gl_vertex_attrib {
    bool        enabled;
    GLint       size;
    GLenum      type;
    GLboolean   normalized;
    GLsizei     stride;
    const void *pointer;
    GLuint      buffer_id;
};

struct gl_state_machine {
    struct gl_buffer_obj    buffers[MAX_GL_BUFFERS];
    struct gl_shader_obj    shaders[MAX_GL_SHADERS];
    struct gl_program_obj   programs[MAX_GL_PROGRAMS];

    GLuint                  current_array_buffer;
    GLuint                  current_element_buffer;
    GLuint                  current_program;

    struct gl_vertex_attrib attribs[MAX_VERTEX_ATTRIBS];

    bool                    depth_test_enabled;
    bool                    blend_enabled;
    GLenum                  blend_src;
    GLenum                  blend_dst;

    struct pipe_context    *pipe;
};

static struct gl_state_machine s_gl_state;

void gl_state_set_pipe(struct pipe_context *p)
{
    s_gl_state.pipe = p;
}

/* =========================================================================
 * 1. Viewport, Scissor & Clear
 * ========================================================================= */
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (s_gl_state.pipe) {
        s_gl_state.pipe->viewport_x = (uint32_t)x;
        s_gl_state.pipe->viewport_y = (uint32_t)y;
        s_gl_state.pipe->viewport_w = (uint32_t)width;
        s_gl_state.pipe->viewport_h = (uint32_t)height;
    }
}

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    (void)x; (void)y; (void)width; (void)height;
}

void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
    uint32_t r = (uint32_t)(red * 255.0f);
    uint32_t g = (uint32_t)(green * 255.0f);
    uint32_t b = (uint32_t)(blue * 255.0f);
    uint32_t a = (uint32_t)(alpha * 255.0f);
    if (s_gl_state.pipe) {
        s_gl_state.pipe->clear_color = (a << 24) | (r << 16) | (g << 8) | b;
    }
}

void glClearDepthf(GLclampf depth)
{
    if (s_gl_state.pipe) {
        s_gl_state.pipe->clear_depth = depth;
    }
}

void glClear(GLbitfield mask)
{
    if (s_gl_state.pipe && s_gl_state.pipe->clear) {
        s_gl_state.pipe->clear(s_gl_state.pipe, mask);
    }
}

/* =========================================================================
 * 2. Capabilities & Blend
 * ========================================================================= */
void glEnable(GLenum cap)
{
    if (cap == GL_DEPTH_TEST) s_gl_state.depth_test_enabled = true;
    if (cap == GL_BLEND) s_gl_state.blend_enabled = true;
}

void glDisable(GLenum cap)
{
    if (cap == GL_DEPTH_TEST) s_gl_state.depth_test_enabled = false;
    if (cap == GL_BLEND) s_gl_state.blend_enabled = false;
}

GLboolean glIsEnabled(GLenum cap)
{
    if (cap == GL_DEPTH_TEST) return s_gl_state.depth_test_enabled ? GL_TRUE : GL_FALSE;
    if (cap == GL_BLEND) return s_gl_state.blend_enabled ? GL_TRUE : GL_FALSE;
    return GL_FALSE;
}

void glBlendFunc(GLenum sfactor, GLenum dfactor)
{
    s_gl_state.blend_src = sfactor;
    s_gl_state.blend_dst = dfactor;
}

void glDepthFunc(GLenum func)
{
    (void)func;
}

/* =========================================================================
 * 3. Vertex Buffer Objects (VBO)
 * ========================================================================= */
void glGenBuffers(GLsizei n, GLuint *buffers)
{
    if (!buffers) return;
    int allocated = 0;
    for (int i = 1; i < MAX_GL_BUFFERS && allocated < n; i++) {
        if (s_gl_state.buffers[i].id == 0) {
            s_gl_state.buffers[i].id = i;
            buffers[allocated++] = i;
        }
    }
}

void glBindBuffer(GLenum target, GLuint buffer)
{
    if (target == GL_ARRAY_BUFFER) {
        s_gl_state.current_array_buffer = buffer;
    } else if (target == GL_ELEMENT_ARRAY_BUFFER) {
        s_gl_state.current_element_buffer = buffer;
    }
}

void glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage)
{
    GLuint buf_id = (target == GL_ARRAY_BUFFER) ? s_gl_state.current_array_buffer : s_gl_state.current_element_buffer;
    if (buf_id == 0 || buf_id >= MAX_GL_BUFFERS) return;

    struct gl_buffer_obj *b = &s_gl_state.buffers[buf_id];
    if (b->data) kfree(b->data);

    b->data = kzalloc(size, GFP_KERNEL);
    if (!b->data) return;

    if (data) memcpy(b->data, data, size);
    b->size = size;
    b->usage = usage;
    b->target = target;
}

void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data)
{
    GLuint buf_id = (target == GL_ARRAY_BUFFER) ? s_gl_state.current_array_buffer : s_gl_state.current_element_buffer;
    if (buf_id == 0 || buf_id >= MAX_GL_BUFFERS || !data) return;

    struct gl_buffer_obj *b = &s_gl_state.buffers[buf_id];
    if (b->data && (offset + size <= (GLintptr)b->size)) {
        memcpy((uint8_t *)b->data + offset, data, size);
    }
}

void glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
    if (!buffers) return;
    for (int i = 0; i < n; i++) {
        GLuint id = buffers[i];
        if (id > 0 && id < MAX_GL_BUFFERS) {
            if (s_gl_state.buffers[id].data) kfree(s_gl_state.buffers[id].data);
            memset(&s_gl_state.buffers[id], 0, sizeof(struct gl_buffer_obj));
        }
    }
}

/* =========================================================================
 * 4. Vertex Attributes
 * ========================================================================= */
void glEnableVertexAttribArray(GLuint index)
{
    if (index < MAX_VERTEX_ATTRIBS) {
        s_gl_state.attribs[index].enabled = true;
    }
}

void glDisableVertexAttribArray(GLuint index)
{
    if (index < MAX_VERTEX_ATTRIBS) {
        s_gl_state.attribs[index].enabled = false;
    }
}

void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer)
{
    if (index >= MAX_VERTEX_ATTRIBS) return;

    s_gl_state.attribs[index].size = size;
    s_gl_state.attribs[index].type = type;
    s_gl_state.attribs[index].normalized = normalized;
    s_gl_state.attribs[index].stride = stride ? (uint32_t)stride : (uint32_t)(size * sizeof(float));
    s_gl_state.attribs[index].pointer = pointer;
    s_gl_state.attribs[index].buffer_id = s_gl_state.current_array_buffer;
}

/* =========================================================================
 * 5. Shaders & Programs
 * ========================================================================= */
GLuint glCreateShader(GLenum type)
{
    for (int i = 1; i < MAX_GL_SHADERS; i++) {
        if (s_gl_state.shaders[i].id == 0) {
            s_gl_state.shaders[i].id = i;
            s_gl_state.shaders[i].type = type;
            return i;
        }
    }
    return 0;
}

void glShaderSource(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length)
{
    (void)length;
    if (shader == 0 || shader >= MAX_GL_SHADERS || !string || count <= 0) return;
    struct gl_shader_obj *s = &s_gl_state.shaders[shader];
    if (s->source) kfree(s->source);

    size_t len = strlen(string[0]) + 1;
    s->source = (char *)kmalloc(len, GFP_KERNEL);
    if (s->source) memcpy(s->source, string[0], len);
}

void glCompileShader(GLuint shader)
{
    if (shader > 0 && shader < MAX_GL_SHADERS) {
        s_gl_state.shaders[shader].compiled = true;
    }
}

void glGetShaderiv(GLuint shader, GLenum pname, GLint *params)
{
    if (!params || shader == 0 || shader >= MAX_GL_SHADERS) return;
    if (pname == GL_COMPILE_STATUS) {
        *params = s_gl_state.shaders[shader].compiled ? GL_TRUE : GL_FALSE;
    }
}

void glGetShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *infolog)
{
    (void)shader; (void)bufsize;
    if (length) *length = 0;
    if (infolog) infolog[0] = '\0';
}

void glDeleteShader(GLuint shader)
{
    if (shader > 0 && shader < MAX_GL_SHADERS) {
        if (s_gl_state.shaders[shader].source) kfree(s_gl_state.shaders[shader].source);
        memset(&s_gl_state.shaders[shader], 0, sizeof(struct gl_shader_obj));
    }
}

GLuint glCreateProgram(void)
{
    for (int i = 1; i < MAX_GL_PROGRAMS; i++) {
        if (s_gl_state.programs[i].id == 0) {
            s_gl_state.programs[i].id = i;
            /* Identity MVP matrix default */
            for (int m = 0; m < 16; m++) s_gl_state.programs[i].mvp[m] = (m % 5 == 0) ? 1.0f : 0.0f;
            return i;
        }
    }
    return 0;
}

void glAttachShader(GLuint program, GLuint shader)
{
    if (program == 0 || program >= MAX_GL_PROGRAMS) return;
    if (shader == 0 || shader >= MAX_GL_SHADERS) return;

    if (s_gl_state.shaders[shader].type == GL_VERTEX_SHADER) {
        s_gl_state.programs[program].vert_shader = shader;
    } else {
        s_gl_state.programs[program].frag_shader = shader;
    }
}

void glLinkProgram(GLuint program)
{
    if (program > 0 && program < MAX_GL_PROGRAMS) {
        s_gl_state.programs[program].linked = true;
    }
}

void glGetProgramiv(GLuint program, GLenum pname, GLint *params)
{
    if (!params || program == 0 || program >= MAX_GL_PROGRAMS) return;
    if (pname == GL_LINK_STATUS) {
        *params = s_gl_state.programs[program].linked ? GL_TRUE : GL_FALSE;
    }
}

void glGetProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei *length, GLchar *infolog)
{
    (void)program; (void)bufsize;
    if (length) *length = 0;
    if (infolog) infolog[0] = '\0';
}

void glUseProgram(GLuint program)
{
    s_gl_state.current_program = program;
}

void glDeleteProgram(GLuint program)
{
    if (program > 0 && program < MAX_GL_PROGRAMS) {
        memset(&s_gl_state.programs[program], 0, sizeof(struct gl_program_obj));
    }
}

GLint glGetAttribLocation(GLuint program, const GLchar *name)
{
    (void)program;
    if (strstr(name, "Position") || strstr(name, "pos")) return 0;
    if (strstr(name, "Color") || strstr(name, "col")) return 1;
    if (strstr(name, "TexCoord") || strstr(name, "tex")) return 2;
    return 0;
}

GLint glGetUniformLocation(GLuint program, const GLchar *name)
{
    (void)program;
    if (strstr(name, "ModelViewProjection") || strstr(name, "mvp") || strstr(name, "Matrix")) return 0;
    if (strstr(name, "Color") || strstr(name, "color")) return 1;
    return 0;
}

void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
    if (s_gl_state.current_program > 0) {
        s_gl_state.programs[s_gl_state.current_program].uniform_color[0] = v0;
        s_gl_state.programs[s_gl_state.current_program].uniform_color[1] = v1;
        s_gl_state.programs[s_gl_state.current_program].uniform_color[2] = v2;
        s_gl_state.programs[s_gl_state.current_program].uniform_color[3] = v3;
    }
    (void)location;
}

void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    (void)location; (void)count; (void)transpose;
    if (s_gl_state.current_program > 0 && value) {
        memcpy(s_gl_state.programs[s_gl_state.current_program].mvp, value, 16 * sizeof(float));
    }
}

/* =========================================================================
 * 6. Primitive Drawing Pipeline
 * ========================================================================= */
void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    (void)mode;
    if (count <= 0 || !s_gl_state.pipe) return;

    /* Assemble vertex buffer: each vertex [X, Y, Z, R, G, B] */
    float *vertex_stream = (float *)kmalloc(count * 6 * sizeof(float), GFP_KERNEL);
    if (!vertex_stream) return;

    /* Fetch position from attrib 0 */
    struct gl_vertex_attrib *a_pos = &s_gl_state.attribs[0];
    const uint8_t *pos_base = NULL;
    if (a_pos->buffer_id > 0 && a_pos->buffer_id < MAX_GL_BUFFERS) {
        pos_base = (const uint8_t *)s_gl_state.buffers[a_pos->buffer_id].data;
    } else {
        pos_base = (const uint8_t *)a_pos->pointer;
    }

    /* Fetch color from attrib 1 */
    struct gl_vertex_attrib *a_col = &s_gl_state.attribs[1];
    const uint8_t *col_base = NULL;
    if (a_col->buffer_id > 0 && a_col->buffer_id < MAX_GL_BUFFERS) {
        col_base = (const uint8_t *)s_gl_state.buffers[a_col->buffer_id].data;
    } else {
        col_base = (const uint8_t *)a_col->pointer;
    }

    for (int i = 0; i < count; i++) {
        int v_idx = first + i;

        /* Position (X, Y, Z) */
        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (pos_base) {
            const float *p = (const float *)(pos_base + (v_idx * a_pos->stride));
            x = p[0];
            y = (a_pos->size > 1) ? p[1] : 0.0f;
            z = (a_pos->size > 2) ? p[2] : 0.0f;
        }

        /* Color (R, G, B) */
        float r = 1.0f, g = 1.0f, b = 1.0f;
        if (col_base) {
            const float *c = (const float *)(col_base + (v_idx * a_col->stride));
            r = c[0];
            g = (a_col->size > 1) ? c[1] : 0.0f;
            b = (a_col->size > 2) ? c[2] : 0.0f;
        }

        vertex_stream[i * 6 + 0] = x;
        vertex_stream[i * 6 + 1] = y;
        vertex_stream[i * 6 + 2] = z;
        vertex_stream[i * 6 + 3] = r;
        vertex_stream[i * 6 + 4] = g;
        vertex_stream[i * 6 + 5] = b;
    }

    /* Dispatch to Gallium Driver */
    if (s_gl_state.pipe->draw_triangles) {
        s_gl_state.pipe->draw_triangles(s_gl_state.pipe, vertex_stream, count);
    }

    kfree(vertex_stream);
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    (void)mode; (void)type;
    glDrawArrays(GL_TRIANGLES, 0, count);
    (void)indices;
}

void glFlush(void) {}
void glFinish(void) {}

const GLubyte *glGetString(GLenum name)
{
    switch (name) {
    case GL_VENDOR:
        return (const GLubyte *)"CherryOS / Mesa Project";
    case GL_RENDERER:
        return (const GLubyte *)"Mali-G31 (Panfrost) / VirtIO-GPU VirGL";
    case GL_VERSION:
        return (const GLubyte *)"OpenGL ES 2.0 CherryMesa 24.0.0";
    case GL_SHADING_LANGUAGE_VERSION:
        return (const GLubyte *)"OpenGL ES GLSL ES 1.00";
    case GL_EXTENSIONS:
        return (const GLubyte *)"GL_OES_texture_npot GL_OES_rgb8_rgba8 GL_OES_depth24";
    default:
        return NULL;
    }
}

void glGetIntegerv(GLenum pname, GLint *params)
{
    if (!params) return;
    if (pname == GL_VIEWPORT && s_gl_state.pipe) {
        params[0] = s_gl_state.pipe->viewport_x;
        params[1] = s_gl_state.pipe->viewport_y;
        params[2] = s_gl_state.pipe->viewport_w;
        params[3] = s_gl_state.pipe->viewport_h;
    }
}
