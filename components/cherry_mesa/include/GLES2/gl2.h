/*
 * CherryMesa - OpenGL ES 2.0 Specification Header
 *
 * Upstream Mesa Custom-Tailored GLES2 API
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_GL2_H_
#define _CHERRY_GL2_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void             GLvoid;
typedef char             GLchar;
typedef unsigned int     GLenum;
typedef unsigned char    GLboolean;
typedef unsigned int     GLbitfield;
typedef int8_t           GLbyte;
typedef int16_t          GLshort;
typedef int32_t          GLint;
typedef int32_t          GLsizei;
typedef uint8_t          GLubyte;
typedef uint16_t         GLushort;
typedef uint32_t         GLuint;
typedef float            GLfloat;
typedef float            GLclampf;
typedef ptrdiff_t        GLsizeiptr;
typedef intptr_t         GLintptr;

#define GL_FALSE                                0
#define GL_TRUE                                 1

#define GL_VIEWPORT                             0x0BA2

#define GL_BYTE                                 0x1400
#define GL_UNSIGNED_BYTE                        0x1401
#define GL_SHORT                                0x1402
#define GL_UNSIGNED_SHORT                       0x1403
#define GL_INT                                  0x1404
#define GL_UNSIGNED_INT                         0x1405
#define GL_FLOAT                                0x1406
#define GL_FIXED                                0x140C

#define GL_POINTS                               0x0000
#define GL_LINES                                0x0001
#define GL_LINE_LOOP                            0x0002
#define GL_LINE_STRIP                           0x0003
#define GL_TRIANGLES                            0x0004
#define GL_TRIANGLE_STRIP                       0x0005
#define GL_TRIANGLE_FAN                         0x0006

#define GL_NEVER                                0x0200
#define GL_LESS                                 0x0201
#define GL_EQUAL                                0x0202
#define GL_LEQUAL                               0x0203
#define GL_GREATER                              0x0204
#define GL_NOTEQUAL                             0x0205
#define GL_GEQUAL                               0x0206
#define GL_ALWAYS                               0x0207

#define GL_SRC_COLOR                            0x0300
#define GL_ONE_MINUS_SRC_COLOR                  0x0301
#define GL_SRC_ALPHA                            0x0302
#define GL_ONE_MINUS_SRC_ALPHA                  0x0303
#define GL_DST_ALPHA                            0x0304
#define GL_ONE_MINUS_DST_ALPHA                  0x0305
#define GL_DST_COLOR                            0x0306
#define GL_ONE_MINUS_DST_COLOR                  0x0307
#define GL_SRC_ALPHA_SATURATE                   0x0308
#define GL_ZERO                                 0
#define GL_ONE                                  1

#define GL_DEPTH_BUFFER_BIT                     0x00000100
#define GL_STENCIL_BUFFER_BIT                   0x00000400
#define GL_COLOR_BUFFER_BIT                     0x00004000

#define GL_TEXTURE_2D                           0x0DE1
#define GL_CULL_FACE                            0x0B44
#define GL_BLEND                                0x0BE2
#define GL_DITHER                               0x0BD0
#define GL_STENCIL_TEST                         0x0B90
#define GL_DEPTH_TEST                           0x0B71
#define GL_SCISSOR_TEST                         0x0C11

#define GL_RGB                                  0x1907
#define GL_RGBA                                 0x1908
#define GL_LUMINANCE                            0x1909
#define GL_LUMINANCE_ALPHA                      0x190A

#define GL_ARRAY_BUFFER                         0x8892
#define GL_ELEMENT_ARRAY_BUFFER                 0x8893
#define GL_ARRAY_BUFFER_BINDING                 0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING         0x8895

#define GL_STREAM_DRAW                          0x88E0
#define GL_STATIC_DRAW                          0x88E4
#define GL_DYNAMIC_DRAW                         0x88E8

#define GL_FRAGMENT_SHADER                      0x8B30
#define GL_VERTEX_SHADER                        0x8B31
#define GL_COMPILE_STATUS                       0x8B81
#define GL_LINK_STATUS                          0x8B82
#define GL_INFO_LOG_LENGTH                      0x8B84

#define GL_VENDOR                               0x1F00
#define GL_RENDERER                             0x1F01
#define GL_VERSION                              0x1F02
#define GL_EXTENSIONS                           0x1F03
#define GL_SHADING_LANGUAGE_VERSION             0x8B8C

#define GL_TEXTURE_MAG_FILTER                   0x2800
#define GL_TEXTURE_MIN_FILTER                   0x2801
#define GL_TEXTURE_WRAP_S                       0x2802
#define GL_TEXTURE_WRAP_T                       0x2803
#define GL_NEAREST                              0x2600
#define GL_LINEAR                               0x2601

/* Function Prototypes */
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void glClearDepthf(GLclampf depth);
void glClear(GLbitfield mask);

void glEnable(GLenum cap);
void glDisable(GLenum cap);
GLboolean glIsEnabled(GLenum cap);
void glBlendFunc(GLenum sfactor, GLenum dfactor);
void glDepthFunc(GLenum func);

void glGenBuffers(GLsizei n, GLuint *buffers);
void glBindBuffer(GLenum target, GLuint buffer);
void glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data);
void glDeleteBuffers(GLsizei n, const GLuint *buffers);

void glEnableVertexAttribArray(GLuint index);
void glDisableVertexAttribArray(GLuint index);
void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);

GLuint glCreateShader(GLenum type);
void glShaderSource(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
void glCompileShader(GLuint shader);
void glGetShaderiv(GLuint shader, GLenum pname, GLint *params);
void glGetShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *infolog);
void glDeleteShader(GLuint shader);

GLuint glCreateProgram(void);
void glAttachShader(GLuint program, GLuint shader);
void glLinkProgram(GLuint program);
void glGetProgramiv(GLuint program, GLenum pname, GLint *params);
void glGetProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei *length, GLchar *infolog);
void glUseProgram(GLuint program);
void glDeleteProgram(GLuint program);

GLint glGetAttribLocation(GLuint program, const GLchar *name);
GLint glGetUniformLocation(GLuint program, const GLchar *name);
void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);

void glGenTextures(GLsizei n, GLuint *textures);
void glBindTexture(GLenum target, GLuint texture);
void glTexParameteri(GLenum target, GLenum pname, GLint param);
void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void glDeleteTextures(GLsizei n, const GLuint *textures);

void glDrawArrays(GLenum mode, GLint first, GLsizei count);
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);

void glFlush(void);
void glFinish(void);

const GLubyte *glGetString(GLenum name);
void glGetIntegerv(GLenum pname, GLint *params);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_GL2_H_ */
