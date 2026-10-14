/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Master Header File
 */

#ifndef _CHERRY_VFS_H_
#define _CHERRY_VFS_H_

#include "vfs_types.h"
#include "vfs_inode.h"
#include "vfs_dev.h"
#include "vfs_driver.h"
#include "vfs_platform.h"
#include "vfs_core.h"
#include "vfs_posix.h"

#ifdef __cplusplus
extern "C" {
#endif

/* System-level initialization of CherryVFS framework */
int cherry_vfs_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_VFS_H_ */
