/*
 * CherryExt4 - Public API & Subsystem Initialization
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef CHERRY_EXT4_H
#define CHERRY_EXT4_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize CherryExt4 subsystem and register "ext4" filesystem to CherryVFS */
int cherry_ext4_init(void);

/* Format block device with Ext4 filesystem */
int ext4_mkfs(const char *dev_name, const char *vol_name);

/* Check and repair Ext4 filesystem consistency */
int ext4_fsck(const char *dev_name, bool repair);

/* Acceptance test suite */
void cherry_ext4_run_tests(void);

#ifdef __cplusplus
}
#endif

#endif /* CHERRY_EXT4_H */
