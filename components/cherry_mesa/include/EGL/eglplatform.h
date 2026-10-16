/*
 * CherryMesa - EGL Platform Definitions
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_EGLPLATFORM_H_
#define _CHERRY_EGLPLATFORM_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *EGLNativeDisplayType;
typedef void *EGLNativePixmapType;
typedef void *EGLNativeWindowType;

#define EGLAPI
#define EGLAPIENTRY

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_EGLPLATFORM_H_ */
